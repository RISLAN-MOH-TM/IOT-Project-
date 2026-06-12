#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ═══════════════════════════════════════════════════════════
// 🔧 ESP32 #1 - SENSOR NODE (WiFi Communication)
// ═══════════════════════════════════════════════════════════
// Monitors traffic density and emergency vehicle detection
// Sends data to ESP32 #2 via WiFi HTTP (NO WIRES NEEDED!)

// ⚠️ WIFI CONFIGURATION ⚠️
const char* ssid = "Redmi";
const char* password = "1234567890";

// ⚠️ ESP32 #2 IP ADDRESS - UPDATE THIS! ⚠️
// Look at ESP32 #2 Serial Monitor to get the IP address
String esp32_2_ip = "192.168.0.136";  // CHANGE THIS!

// ═══════════════════════════════════════════════════════════
// 📡 RFID READER PINS (4 READERS - VSPI)
// ═══════════════════════════════════════════════════════════
#define RST_PIN 22
#define SS_1_PIN 15  // Lane 1
#define SS_2_PIN 2   // Lane 2
#define SS_3_PIN 21  // Lane 3
#define SS_4_PIN 4   // Lane 4

// ═══════════════════════════════════════════════════════════
// 🚗 IR SENSOR PINS (4 LANES)
// ═══════════════════════════════════════════════════════════
int irPins[4] = {32, 33, 25, 26};

// IR sensor debouncing
const unsigned long IR_DEBOUNCE_TIME = 300;
unsigned long lastIRChange[4] = {0, 0, 0, 0};
int lastIRState[4] = {HIGH, HIGH, HIGH, HIGH};
int stableIRState[4] = {HIGH, HIGH, HIGH, HIGH};

// ═══════════════════════════════════════════════════════════
// 🚑 EMERGENCY RFID CONFIGURATION
// ═══════════════════════════════════════════════════════════
byte emergencyUID[4] = {0x53, 0x16, 0x7A, 0x2D};  // UID: 53 16 7A 2D

int emergency = 0;
unsigned long lastEmergencyTime[4] = {0, 0, 0, 0};
const unsigned long EMERGENCY_COOLDOWN = 3000;

// ═══════════════════════════════════════════════════════════
// 📊 TRAFFIC DENSITY VARIABLES
// ═══════════════════════════════════════════════════════════
unsigned long densityStartTime[4] = {0, 0, 0, 0};
unsigned long densityDuration[4] = {0, 0, 0, 0};
int trafficDensity[4] = {0, 0, 0, 0};
int vehicleCount[4] = {0, 0, 0, 0};

unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 500;  // Send every 500ms

// RFID Readers
MFRC522 mfrc522[4] = {
  MFRC522(SS_1_PIN, RST_PIN),
  MFRC522(SS_2_PIN, RST_PIN),
  MFRC522(SS_3_PIN, RST_PIN),
  MFRC522(SS_4_PIN, RST_PIN)
};

// ═══════════════════════════════════════════════════════════
// 📡 WIFI COMMUNICATION
// ═══════════════════════════════════════════════════════════
void sendDataToESP32_2() {
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi not connected!");
    return;
  }
  
  HTTPClient http;
  
  // Build data string
  String data = "L1:" + String(trafficDensity[0]) + ",";
  data += "L2:" + String(trafficDensity[1]) + ",";
  data += "L3:" + String(trafficDensity[2]) + ",";
  data += "L4:" + String(trafficDensity[3]) + ",";
  data += "EMG:" + String(emergency) + ",";
  data += "C1:" + String(vehicleCount[0]) + ",";
  data += "C2:" + String(vehicleCount[1]) + ",";
  data += "C3:" + String(vehicleCount[2]) + ",";
  data += "C4:" + String(vehicleCount[3]);
  
  // Send to ESP32 #2
  String url = "http://" + esp32_2_ip + "/sensorData?data=" + data;
  
  http.begin(url);
  http.setTimeout(1000);  // 1 second timeout
  
  int httpCode = http.GET();
  
  if(httpCode > 0) {
    if(httpCode == 200) {
      Serial.println("✅ Data sent successfully");
    } else {
      Serial.print("⚠️ HTTP Error: ");
      Serial.println(httpCode);
    }
  } else {
    Serial.println("❌ Connection failed to ESP32 #2");
  }
  
  http.end();
}

// ═══════════════════════════════════════════════════════════
// 🔧 SETUP
// ═══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n╔═══════════════════════════════════════╗");
  Serial.println("║  Smart Traffic Management System     ║");
  Serial.println("║  ESP32 #1 - SENSOR NODE (WiFi)       ║");
  Serial.println("╚═══════════════════════════════════════╝");
  
  // Connect to WiFi
  Serial.println("\n📡 Connecting to WiFi...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  Serial.println();
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ WiFi Connected!");
    Serial.print("My IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("ESP32 #2 IP: ");
    Serial.println(esp32_2_ip);
    Serial.println("⚠️ Make sure ESP32 #2 IP is correct!");
  } else {
    Serial.println("❌ WiFi Connection Failed!");
    Serial.println("System will not work without WiFi!");
  }
  
  // Initialize SPI for RFID readers
  SPI.begin();
  Serial.println("\n✅ SPI initialized");
  
  // Initialize RFID readers
  Serial.println("🚑 Initializing RFID Readers...");
  for(int i = 0; i < 4; i++) {
    mfrc522[i].PCD_Init();
    Serial.print("  ✅ Reader ");
    Serial.print(i + 1);
    Serial.println(" ready");
    delay(100);
  }
  
  // Initialize IR sensors
  Serial.println("🚗 Initializing IR Sensors...");
  for(int i = 0; i < 4; i++) {
    pinMode(irPins[i], INPUT_PULLUP);
    Serial.print("  ✅ IR Sensor Lane ");
    Serial.print(i + 1);
    Serial.print(" (GPIO ");
    Serial.print(irPins[i]);
    Serial.println(") ready");
  }
  
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║  🚑 EMERGENCY CONFIGURATION          ║");
  Serial.println("╠═══════════════════════════════════════╣");
  Serial.println("║  Mode: SPECIFIC UID ONLY             ║");
  Serial.print("║  Emergency UID: ");
  for(int i = 0; i < 4; i++) {
    if(emergencyUID[i] < 0x10) Serial.print("0");
    Serial.print(emergencyUID[i], HEX);
    if(i < 3) Serial.print(" ");
  }
  Serial.println("          ║");
  Serial.println("╚═══════════════════════════════════════╝");
  
  Serial.println("\n✅ System Ready!");
  Serial.println("📊 Monitoring traffic density (duration-based)");
  Serial.println("🚑 Emergency: SPECIFIC UID only");
  Serial.println("📡 Communication: WiFi HTTP to ESP32 #2");
  Serial.println("════════════════════════════════════════\n");
}

// ═══════════════════════════════════════════════════════════
// 🚗 IR SENSOR READING WITH DEBOUNCE
// ═══════════════════════════════════════════════════════════
void updateIRSensors() {
  unsigned long currentTime = millis();
  
  for(int i = 0; i < 4; i++) {
    int reading = digitalRead(irPins[i]);
    
    // If state changed, record time
    if(reading != lastIRState[i]) {
      lastIRChange[i] = currentTime;
      lastIRState[i] = reading;
    }
    
    // If state stable for debounce time, accept it
    if((currentTime - lastIRChange[i]) > IR_DEBOUNCE_TIME) {
      if(reading != stableIRState[i]) {
        stableIRState[i] = reading;
        
        // Vehicle detection logic
        if(stableIRState[i] == LOW) {
          // Vehicle detected - start timing
          if(densityStartTime[i] == 0) {
            densityStartTime[i] = currentTime;
            vehicleCount[i]++;
            Serial.print("🚗 Vehicle CONFIRMED at Lane ");
            Serial.print(i + 1);
            Serial.print(" [IR: DETECTED] | Count: ");
            Serial.println(vehicleCount[i]);
          }
        } else {
          // Vehicle left - record duration
          if(densityStartTime[i] > 0) {
            float waitTime = (currentTime - densityStartTime[i]) / 1000.0;
            Serial.print("✓ Vehicle LEFT Lane ");
            Serial.print(i + 1);
            Serial.print(" - Wait time: ");
            Serial.print(waitTime, 1);
            Serial.println(" seconds");
            densityStartTime[i] = 0;
          }
        }
      }
    }
    
    // Update density based on wait duration
    if(stableIRState[i] == LOW && densityStartTime[i] > 0) {
      densityDuration[i] = currentTime - densityStartTime[i];
      trafficDensity[i] = min(100, (int)((densityDuration[i] / 10000.0) * 100));
    } else if(trafficDensity[i] > 0) {
      trafficDensity[i] = max(0, trafficDensity[i] - 5);
    }
  }
}

// ═══════════════════════════════════════════════════════════
// 🚑 RFID EMERGENCY DETECTION
// ═══════════════════════════════════════════════════════════
bool compareUID(byte* buffer, byte* target) {
  for(int i = 0; i < 4; i++) {
    if(buffer[i] != target[i]) return false;
  }
  return true;
}

void checkRFID() {
  unsigned long currentTime = millis();
  
  for(int i = 0; i < 4; i++) {
    if(!mfrc522[i].PICC_IsNewCardPresent() || !mfrc522[i].PICC_ReadCardSerial()) {
      // Check cooldown expiry
      if(lastEmergencyTime[i] > 0 && (currentTime - lastEmergencyTime[i] > EMERGENCY_COOLDOWN)) {
        if(emergency == (i + 1)) {
          emergency = 0;
          Serial.print("⏰ Emergency cooldown expired for Lane ");
          Serial.println(i + 1);
        }
        lastEmergencyTime[i] = 0;
      }
      continue;
    }
    
    Serial.print("📡 RFID detected on Lane ");
    Serial.print(i + 1);
    Serial.print(" - UID: ");
    for(int j = 0; j < mfrc522[i].uid.size; j++) {
      if(mfrc522[i].uid.uidByte[j] < 0x10) Serial.print("0");
      Serial.print(mfrc522[i].uid.uidByte[j], HEX);
      if(j < mfrc522[i].uid.size - 1) Serial.print(" ");
    }
    Serial.println();
    
    // Check if matches emergency UID
    if(compareUID(mfrc522[i].uid.uidByte, emergencyUID)) {
      emergency = i + 1;
      lastEmergencyTime[i] = currentTime;
      
      Serial.println("╔═══════════════════════════════════════╗");
      Serial.println("║  🚑 EMERGENCY VEHICLE DETECTED!      ║");
      Serial.println("╚═══════════════════════════════════════╝");
      Serial.print("  Lane ");
      Serial.print(i + 1);
      Serial.println(" gets IMMEDIATE PRIORITY");
      Serial.println("  All other lanes: RED");
      Serial.println("  Auto-clear in 3 seconds");
      Serial.println("════════════════════════════════════════");
    } else {
      Serial.println("   ❌ NOT emergency vehicle (UID mismatch)");
      Serial.print("   Expected: ");
      for(int j = 0; j < 4; j++) {
        if(emergencyUID[j] < 0x10) Serial.print("0");
        Serial.print(emergencyUID[j], HEX);
        if(j < 3) Serial.print(" ");
      }
      Serial.println();
    }
    
    mfrc522[i].PICC_HaltA();
    mfrc522[i].PCD_StopCrypto1();
  }
}

// ═══════════════════════════════════════════════════════════
// 🔄 MAIN LOOP
// ═══════════════════════════════════════════════════════════
void loop() {
  updateIRSensors();
  checkRFID();
  
  // Send data via WiFi every 500ms
  unsigned long currentTime = millis();
  if(currentTime - lastSendTime >= SEND_INTERVAL) {
    sendDataToESP32_2();
    lastSendTime = currentTime;
    
    // Debug output
    Serial.print("📊 Density: L1:");
    Serial.print(trafficDensity[0]);
    Serial.print("% L2:");
    Serial.print(trafficDensity[1]);
    Serial.print("% L3:");
    Serial.print(trafficDensity[2]);
    Serial.print("% L4:");
    Serial.print(trafficDensity[3]);
    Serial.print("% | EMG: ");
    Serial.println(emergency);
  }
}
