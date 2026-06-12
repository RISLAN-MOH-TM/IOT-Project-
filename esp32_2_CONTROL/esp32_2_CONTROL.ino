#include <WiFi.h>
#include <WebServer.h>

// ═══════════════════════════════════════════════════════════
// 🔧 ESP32 #2 - CONTROL NODE
// ═══════════════════════════════════════════════════════════
// This ESP32 receives data from ESP32 #1, controls traffic lights,
// manages street lights, and serves the web dashboard

// ⚠️ REPLACE WITH YOUR WIFI CREDENTIALS ⚠️
const char* ssid = "Redmi";
const char* password = "1234567890";

WebServer server(80);

// ═══════════════════════════════════════════════════════════
// 📊 TRAFFIC DATA
// ═══════════════════════════════════════════════════════════
int L[4] = {0, 0, 0, 0};        // Density scores from ESP32 #1
int C[4] = {0, 0, 0, 0};        // Vehicle counts from ESP32 #1
int emergency = 0;               // Emergency lane (0 = none, 1-4 = lane)
int activeLane = 1;              // Current active green lane
bool manualEmergencyOff = false; // Manual emergency override
bool manualMode = false;         // Manual traffic control mode
int manualLane = 0;              // Manually selected lane (0 = none)

// Priority threshold
const int PRIORITY_THRESHOLD = 40;  // 40% density required for priority

// Communication diagnostics
unsigned long lastDataReceived = 0;
int dataPacketsReceived = 0;

// ═══════════════════════════════════════════════════════════
// 🚦 PHYSICAL TRAFFIC LIGHT GPIO PINS
// ═══════════════════════════════════════════════════════════
int redPins[4]    = {13, 14, 27, 26};  // Red LEDs for lanes 1-4
int yellowPins[4] = {12, 15, 33, 25};  // Yellow LEDs for lanes 1-4
int greenPins[4]  = {32, 2, 4, 5};     // Green LEDs for lanes 1-4

// ═══════════════════════════════════════════════════════════
// 💡 STREET LIGHT SYSTEM PINS
// ═══════════════════════════════════════════════════════════
#define LDR_PIN 34           // LDR sensor (Analog input)
#define STREET_LIGHT_PIN 19  // Street light LED output

// LDR threshold values (adjust based on your sensor)
// NOTE: This is for sensors where HIGHER value = DARKER
#define DARK_THRESHOLD 2000   // Above this = dark (turn light ON)
#define LIGHT_THRESHOLD 1500  // Below this = bright (turn light OFF)

// Street light control variables
int ldrValue = 0;
bool streetLightState = false;
bool autoMode = true;
bool manualOverride = false;

// ═══════════════════════════════════════════════════════════
// ⏱️ TIMER VARIABLES
// ═══════════════════════════════════════════════════════════
const unsigned long NORMAL_GREEN_TIME = 10000;  // 10 seconds per lane
const unsigned long YELLOW_TIME = 2000;         // 2 seconds yellow warning
unsigned long lastLaneChange = 0;
unsigned long yellowStartTime = 0;
int remainingTime = 10;
bool isYellowPhase = false;

// ═══════════════════════════════════════════════════════════
// 🚦 TRAFFIC LIGHT CONTROL FUNCTIONS
// ═══════════════════════════════════════════════════════════
void setTrafficLight(int lane, String color) {
  int idx = lane - 1;
  
  digitalWrite(redPins[idx], LOW);
  digitalWrite(yellowPins[idx], LOW);
  digitalWrite(greenPins[idx], LOW);
  
  if(color == "RED") {
    digitalWrite(redPins[idx], HIGH);
  } else if(color == "YELLOW") {
    digitalWrite(yellowPins[idx], HIGH);
  } else if(color == "GREEN") {
    digitalWrite(greenPins[idx], HIGH);
  }
}

void setAllRed() {
  for(int i = 1; i <= 4; i++) {
    setTrafficLight(i, "RED");
  }
}

void updateTrafficLights() {
  for(int i = 1; i <= 4; i++) {
    if(i == activeLane && !isYellowPhase) {
      setTrafficLight(i, "GREEN");
    } else if(i == activeLane && isYellowPhase) {
      setTrafficLight(i, "YELLOW");
    } else {
      setTrafficLight(i, "RED");
    }
  }
}

// ═══════════════════════════════════════════════════════════
// 💡 STREET LIGHT CONTROL FUNCTIONS
// ═══════════════════════════════════════════════════════════
void updateStreetLight() {
  ldrValue = analogRead(LDR_PIN);
  
  if(manualOverride) {
    digitalWrite(STREET_LIGHT_PIN, streetLightState ? HIGH : LOW);
    return;
  }
  
  if(autoMode) {
    // LDR Logic: Higher value = Darker
    // DARK (high LDR value) → Turn ON street light
    if(ldrValue > DARK_THRESHOLD) {
      if(!streetLightState) {  // Only log on state change
        Serial.print("💡 DARK detected (LDR: ");
        Serial.print(ldrValue);
        Serial.println(") → Street light ON");
      }
      streetLightState = true;
      digitalWrite(STREET_LIGHT_PIN, HIGH);
    }
    // BRIGHT (low LDR value) → Turn OFF street light
    else if(ldrValue < LIGHT_THRESHOLD) {
      if(streetLightState) {  // Only log on state change
        Serial.print("☀️ BRIGHT detected (LDR: ");
        Serial.print(ldrValue);
        Serial.println(") → Street light OFF");
      }
      streetLightState = false;
      digitalWrite(STREET_LIGHT_PIN, LOW);
    }
    // Between thresholds: keep current state (hysteresis to prevent flickering)
  }
}

void setStreetLightManual(bool state) {
  manualOverride = true;
  autoMode = false;
  streetLightState = state;
  digitalWrite(STREET_LIGHT_PIN, state ? HIGH : LOW);
}

void setStreetLightAuto() {
  manualOverride = false;
  autoMode = true;
}

// ═══════════════════════════════════════════════════════════
// ⏱️ TIMER CALCULATION
// ═══════════════════════════════════════════════════════════
void updateTimer() {
  unsigned long elapsed = millis() - lastLaneChange;
  unsigned long totalTime = NORMAL_GREEN_TIME + YELLOW_TIME;
  
  // Check if in yellow phase
  if(elapsed >= NORMAL_GREEN_TIME && elapsed < totalTime) {
    isYellowPhase = true;
    remainingTime = (totalTime - elapsed) / 1000;
  } else if(elapsed >= totalTime) {
    remainingTime = 0;
    isYellowPhase = false;
  } else {
    isYellowPhase = false;
    remainingTime = (NORMAL_GREEN_TIME - elapsed) / 1000;
  }
  
  // Ensure timer is never negative
  if(remainingTime < 0) remainingTime = 0;
}

// ═══════════════════════════════════════════════════════════
// 🌐 WEB SERVER HANDLERS
// ═══════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════
// 🌐 WEB SERVER HANDLERS
// ═══════════════════════════════════════════════════════════

// NEW: Receive sensor data from ESP32 #1 via WiFi
void handleSensorData() {
  if(server.hasArg("data")) {
    String s = server.arg("data");
    int newEmergency = 0;
    
    // Parse: L1:xx,L2:xx,L3:xx,L4:xx,EMG:x,C1:xx,C2:xx,C3:xx,C4:xx
    int parsed = sscanf(s.c_str(), "L1:%d,L2:%d,L3:%d,L4:%d,EMG:%d,C1:%d,C2:%d,C3:%d,C4:%d", 
                        &L[0], &L[1], &L[2], &L[3], &newEmergency, &C[0], &C[1], &C[2], &C[3]);
    
    if(parsed >= 5) {
      lastDataReceived = millis();
      dataPacketsReceived++;
      
      Serial.print("📡 Data received from ESP32 #1: ");
      Serial.println(s);
      
      // Update emergency status
      if(!manualEmergencyOff) {
        emergency = newEmergency;
      } else if(newEmergency == 0) {
        manualEmergencyOff = false;
      }
      
      // Find lane with most vehicles ABOVE threshold
      int maxLane = 0;
      int maxVal = PRIORITY_THRESHOLD - 1;
      
      for(int i = 0; i < 4; i++) {
        if(L[i] >= PRIORITY_THRESHOLD && L[i] > maxVal) {
          maxVal = L[i];
          maxLane = i + 1;
        }
      }
      
      // If no lane meets threshold, find highest density anyway
      if(maxLane == 0) {
        maxVal = L[0];
        maxLane = 1;
        for(int i = 1; i < 4; i++) {
          if(L[i] > maxVal) {
            maxVal = L[i];
            maxLane = i + 1;
          }
        }
      }
      
      // Determine active lane based on priority
      int newActiveLane = emergency ? emergency : maxLane;
      
      // Only auto-switch if NOT in manual mode
      if(!manualMode) {
        bool shouldSwitch = false;
        
        // Switch if:
        // 1. Emergency changed
        if(emergency != 0 && emergency != activeLane) {
          shouldSwitch = true;
        }
        // 2. No emergency and a different lane has higher priority
        else if(emergency == 0 && newActiveLane != activeLane && maxVal >= PRIORITY_THRESHOLD) {
          shouldSwitch = true;
        }
        // 3. Timer expired and need to check priorities again
        else if(emergency == 0) {
          unsigned long elapsed = millis() - lastLaneChange;
          if(elapsed >= NORMAL_GREEN_TIME + YELLOW_TIME) {
            shouldSwitch = true;
          }
        }
        
        // Perform lane switch
        if(shouldSwitch) {
          activeLane = newActiveLane;
          lastLaneChange = millis();
          isYellowPhase = false;
          remainingTime = NORMAL_GREEN_TIME / 1000;
          
          if(emergency) {
            Serial.print("🚑 EMERGENCY SWITCH: Lane ");
            Serial.print(activeLane);
            Serial.println(" activated");
          } else if(maxVal >= PRIORITY_THRESHOLD) {
            Serial.print("🔄 PRIORITY SWITCH: Lane ");
            Serial.print(activeLane);
            Serial.print(" activated (Density: ");
            Serial.print(L[activeLane - 1]);
            Serial.println("%)");
          } else {
            Serial.print("⏱️ TIMER SWITCH: Lane ");
            Serial.print(activeLane);
            Serial.print(" activated (Density: ");
            Serial.print(L[activeLane - 1]);
            Serial.println("%)");
          }
        }
        
        // Update yellow phase status
        updateTimer();
        updateTrafficLights();
      }
      
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "text/plain", "OK");
    } else {
      Serial.print("⚠️ Parse error: ");
      Serial.println(s);
      server.send(400, "text/plain", "Parse error");
    }
  } else {
    server.send(400, "text/plain", "Missing data parameter");
  }
}

void handleData() {
  updateTimer();
  
  String json = "{";
  json += "\"lane\":" + String(activeLane) + ",";
  json += "\"emergency\":" + String(emergency) + ",";
  json += "\"timer\":" + String(remainingTime) + ",";
  json += "\"streetLight\":" + String(streetLightState ? 1 : 0) + ",";
  json += "\"ldrValue\":" + String(ldrValue) + ",";
  json += "\"autoMode\":" + String(autoMode ? 1 : 0) + ",";
  json += "\"manualMode\":" + String(manualMode ? 1 : 0) + ",";
  json += "\"vehicles\":[" + String(L[0]) + "," + String(L[1]) + "," + String(L[2]) + "," + String(L[3]) + "],";
  json += "\"counts\":[" + String(C[0]) + "," + String(C[1]) + "," + String(C[2]) + "," + String(C[3]) + "]";
  json += "}";
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleManualLane() {
  if(server.hasArg("lane")) {
    int lane = server.arg("lane").toInt();
    if(lane >= 1 && lane <= 4) {
      manualMode = true;
      manualLane = lane;
      activeLane = lane;
      lastLaneChange = millis();
      isYellowPhase = false;
      remainingTime = NORMAL_GREEN_TIME / 1000;
      updateTrafficLights();
      
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "text/plain", "Manual mode: Lane " + String(lane) + " activated");
      
      Serial.print("🎛️ MANUAL CONTROL: Lane ");
      Serial.print(lane);
      Serial.println(" activated");
    } else {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(400, "text/plain", "Invalid lane number");
    }
  } else {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(400, "text/plain", "Missing lane parameter");
  }
}

void handleAutoMode() {
  manualMode = false;
  manualLane = 0;
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Auto mode enabled");
  
  Serial.println("🤖 AUTO MODE: Traffic control restored to automatic");
}

void handleAllRed() {
  manualMode = true;
  manualLane = 0;
  setAllRed();
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "All lanes set to RED");
  
  Serial.println("🛑 ALL RED: All lanes stopped");
}

void handleManualEmergency() {
  if(server.hasArg("lane")) {
    int lane = server.arg("lane").toInt();
    if(lane >= 1 && lane <= 4) {
      emergency = lane;
      manualEmergencyOff = false;
      activeLane = lane;
      lastLaneChange = millis();
      isYellowPhase = false;
      remainingTime = NORMAL_GREEN_TIME / 1000;
      updateTrafficLights();
      
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "text/plain", "Emergency mode: Lane " + String(lane));
      
      Serial.print("🚑 MANUAL EMERGENCY: Lane ");
      Serial.print(lane);
      Serial.println(" activated");
    } else {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(400, "text/plain", "Invalid lane number");
    }
  } else {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(400, "text/plain", "Missing lane parameter");
  }
}

void handleEmergencyOff() {
  manualEmergencyOff = true;
  emergency = 0;
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Emergency mode cleared");
  
  Serial.println("🚑 Emergency mode cleared manually");
}

void handleStreetLightON() {
  setStreetLightManual(true);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Street light turned ON");
}

void handleStreetLightOFF() {
  setStreetLightManual(false);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Street light turned OFF");
}

void handleStreetLightAuto() {
  setStreetLightAuto();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Street light set to AUTO mode");
}

// ═══════════════════════════════════════════════════════════
// 🔧 SETUP
// ═══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=================================");
  Serial.println("Smart Traffic Management System");
  Serial.println("ESP32 #2 - CONTROL NODE (WiFi)");
  Serial.println("=================================");
  
  // NO Serial2 needed - using WiFi!
  Serial.println("📡 Communication: WiFi HTTP (no Serial2 wiring)");
  
  // Initialize traffic light pins
  Serial.println("\nInitializing traffic light pins...");
  for(int i = 0; i < 4; i++) {
    pinMode(redPins[i], OUTPUT);
    pinMode(yellowPins[i], OUTPUT);
    pinMode(greenPins[i], OUTPUT);
  }
  Serial.println("✓ Traffic light pins initialized");
  
  setAllRed();
  Serial.println("✓ All traffic lights set to RED");
  
  // Initialize street light pins
  pinMode(LDR_PIN, INPUT);
  pinMode(STREET_LIGHT_PIN, OUTPUT);
  Serial.println("✓ LDR sensor initialized (GPIO 34)");
  Serial.println("✓ Street light initialized (GPIO 19)");
  Serial.println("💡 LDR Logic: Higher value = Darker");
  Serial.print("  Dark threshold (light ON): >");
  Serial.println(DARK_THRESHOLD);
  Serial.print("  Light threshold (light OFF): <");
  Serial.println(LIGHT_THRESHOLD);
  
  // Connect to WiFi
  Serial.println("\nConnecting to WiFi...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  Serial.println();
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("\n=================================");
    Serial.println("Open this IP in your browser:");
    Serial.print("http://");
    Serial.println(WiFi.localIP());
    Serial.println("=================================\n");
  } else {
    Serial.println("✗ WiFi Connection Failed!");
    Serial.println("Please check your WiFi credentials");
  }
  
  // Setup web server routes
  server.on("/sensorData", handleSensorData);  // NEW: Receive from ESP32 #1
  server.on("/data", handleData);
  server.on("/manualLane", handleManualLane);
  server.on("/autoMode", handleAutoMode);
  server.on("/allRed", handleAllRed);
  server.on("/manualEmergency", handleManualEmergency);
  server.on("/emergencyOff", handleEmergencyOff);
  server.on("/streetON", handleStreetLightON);
  server.on("/streetOFF", handleStreetLightOFF);
  server.on("/streetAuto", handleStreetLightAuto);
  
  server.begin();
  Serial.println("Web server started!");
  Serial.println("\n⚠️  IMPORTANT: WiFi Communication Mode");
  Serial.println("ESP32 #1 will send data via WiFi HTTP");
  Serial.println("NO Serial2 wiring needed!");
  Serial.println("Make sure ESP32 #1 has your IP address configured!");
  Serial.println("════════════════════════════════════════\n");
}

// ═══════════════════════════════════════════════════════════
// 🔄 MAIN LOOP
// ═══════════════════════════════════════════════════════════
void loop() {
  server.handleClient();
  updateStreetLight();
  
  // Check for communication timeout (WiFi mode)
  static unsigned long lastTimeoutCheck = 0;
  if(millis() - lastTimeoutCheck > 5000) {
    if(lastDataReceived > 0 && (millis() - lastDataReceived > 5000)) {
      Serial.println("⚠️ WARNING: No data from ESP32 #1 for 5 seconds!");
      Serial.println("   Check WiFi connection on both ESP32s");
      Serial.println("   Make sure ESP32 #1 has correct IP address");
    } else if(lastDataReceived == 0 && millis() > 30000) {
      Serial.println("⚠️ WARNING: No data received from ESP32 #1 yet!");
      Serial.println("   Make sure ESP32 #1 is powered on and connected to WiFi");
    }
    lastTimeoutCheck = millis();
  }
  
  // ═══════════════════════════════════════════════════════════
  // 🔄 CONTINUOUS AUTO-CYCLING (for demonstration without ESP32 #1)
  // ═══════════════════════════════════════════════════════════
  // If NOT in manual mode and NOT emergency, continuously cycle lanes
  if(!manualMode && emergency == 0) {
    updateTimer();
    
    // Check if timer expired (green + yellow phases complete)
    if(remainingTime <= 0) {
      // Auto-cycle to next lane: 1→2→3→4→1
      activeLane++;
      if(activeLane > 4) {
        activeLane = 1;
      }
      
      // Reset timer and phases
      lastLaneChange = millis();
      isYellowPhase = false;
      remainingTime = NORMAL_GREEN_TIME / 1000;
      
      Serial.print("🔄 AUTO-CYCLE: Lane ");
      Serial.print(activeLane);
      Serial.println(" activated");
      
      // Apply traffic light changes
      updateTrafficLights();
    } else {
      // Update traffic lights during yellow phase
      updateTrafficLights();
    }
  }
}
