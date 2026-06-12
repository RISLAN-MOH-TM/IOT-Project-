# 🚦 Smart Traffic Management System - Separated Architecture

## 📁 File Structure

```
esp32_improved/
├── esp32_1_SENSOR/
│   └── esp32_1_SENSOR.ino       ✅ ESP32 #1 - Sensor Node (Clean C++ only)
├── esp32_2_CONTROL/
│   └── esp32_2_CONTROL.ino      ✅ ESP32 #2 - Control Node (Clean C++ only)
└── README.md                     📖 This file

../Site/
├── index.html                    🌐 Web Dashboard
├── style.css                     🎨 Styles
└── script.js                     ⚡ JavaScript Logic
```

## 🔧 What Changed?

### Before (Had Issues):
- ❌ HTML/JavaScript embedded in Arduino `.ino` files
- ❌ Compilation errors with large raw string literals
- ❌ Hard to maintain and debug

### After (Clean Separation):
- ✅ Pure C++ code in ESP32 files (no web code)
- ✅ Separate standalone website files
- ✅ Easy to compile and maintain
- ✅ Website can be hosted anywhere (ESP32, computer, or server)

## 📋 System Architecture

```
┌─────────────────┐         Serial2          ┌─────────────────┐
│   ESP32 #1      │ ──────────────────────> │   ESP32 #2      │
│  SENSOR NODE    │   (Vehicle data +        │  CONTROL NODE   │
│                 │    Emergency alerts)      │                 │
│ - 4x RFID       │                          │ - Traffic Lights│
│ - 4x IR Sensors │                          │ - Street Light  │
│ - Emergency Tag │                          │ - LDR Sensor    │
└─────────────────┘                          │ - WiFi Server   │
                                              └────────┬────────┘
                                                       │
                                                    WiFi/HTTP
                                                       │
                                              ┌────────▼────────┐
                                              │  Web Dashboard  │
                                              │   (Browser)     │
                                              │                 │
                                              │ - Live Monitor  │
                                              │ - Charts        │
                                              │ - Controls      │
                                              └─────────────────┘
```

## 🚀 Upload Instructions

### Step 1: Upload to ESP32 #1 (Sensor Node)

1. Open Arduino IDE
2. Open file: `esp32_1_SENSOR/esp32_1_SENSOR.ino`
3. Select Board: **ESP32 Dev Module**
4. Select correct COM Port
5. Click **Upload**
6. **Wire connections:**
   - Serial2 TX (GPIO 15) → Connect to ESP32 #2 RX (GPIO 16)
   - Serial2 RX (GPIO 16) → Connect to ESP32 #2 TX (GPIO 17)
   - 4x RFID readers on VSPI (SS pins: 21, 17, 16, 4)
   - 4x IR sensors (GPIO: 32, 33, 25, 26)

### Step 2: Upload to ESP32 #2 (Control Node)

1. Open file: `esp32_2_CONTROL/esp32_2_CONTROL.ino`
2. **IMPORTANT:** Update WiFi credentials:
   ```cpp
   const char* ssid = "YOUR_WIFI_NAME";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
3. Select Board: **ESP32 Dev Module**
4. Select correct COM Port
5. Click **Upload**
6. Open Serial Monitor (115200 baud)
7. **Note the IP address** displayed (e.g., `192.168.1.100`)
8. **Wire connections:**
   - Serial2 RX (GPIO 16) → Connect to ESP32 #1 TX (GPIO 15)
   - Serial2 TX (GPIO 17) → Connect to ESP32 #1 RX (GPIO 16)
   - 12x Traffic light LEDs (R/Y/G for 4 lanes)
   - LDR sensor (GPIO 34)
   - Street light LED (GPIO 19)

### Step 3: Open Web Dashboard

#### Option A: Open Directly from Computer
1. Go to `Site` folder
2. Open `index.html` in your browser
3. In the configuration panel (bottom right):
   - Enter the ESP32 #2 IP address
   - Click "Save & Connect"
4. Dashboard will connect and display live data!

#### Option B: Host on ESP32 (Optional - Future Enhancement)
- Can upload the HTML files to ESP32 SPIFFS later if needed

## 🔌 Wiring Connections

### ESP32 #1 ↔ ESP32 #2 Serial Connection
```
ESP32 #1          ESP32 #2
GPIO 15 (TX) ──→  GPIO 16 (RX)
GPIO 16 (RX) ←──  GPIO 17 (TX)
GND         ────  GND
```

## 📡 API Endpoints (ESP32 #2)

The control node exposes these endpoints:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/data` | GET | Returns JSON with all system data |
| `/streetON` | GET | Turn street lights ON (manual) |
| `/streetOFF` | GET | Turn street lights OFF (manual) |
| `/streetAuto` | GET | Set street lights to AUTO mode |
| `/emergencyOff` | GET | Manually clear emergency mode |

### Example `/data` Response:
```json
{
  "lane": 2,
  "emergency": 0,
  "timer": 8,
  "streetLight": 1,
  "ldrValue": 1200,
  "autoMode": 1,
  "vehicles": [15, 23, 8, 12]
}
```

## 🐛 Troubleshooting

### ESP32 #1 Not Sending Data
- Check Serial2 wiring (TX→RX, RX→TX)
- Verify RFID readers are properly initialized
- Check Serial Monitor for error messages

### ESP32 #2 Not Connecting to WiFi
- Verify WiFi credentials are correct
- Check WiFi signal strength
- Try different WiFi network (2.4GHz only, not 5GHz)

### Website Can't Connect
- Make sure computer and ESP32 are on same WiFi network
- Verify ESP32 IP address is correct
- Check browser console (F12) for error messages
- Disable browser CORS restrictions if needed

### Traffic Lights Not Working
- Check physical LED wiring
- Verify GPIO pin numbers match your hardware
- Use Serial Monitor to see if data is received

## 📊 Features

✅ **Real-time Vehicle Counting** (4 lanes)  
✅ **Emergency Vehicle Priority** (RFID-based)  
✅ **Smart Traffic Light Control**  
✅ **Automatic Street Light System** (LDR sensor)  
✅ **Live Web Dashboard** with charts  
✅ **Manual Override Controls**  
✅ **Traffic Density Visualization**  
✅ **Live Logs & Analytics**

## 🎨 Customization

### Change Traffic Light Timings
Edit in `esp32_2_CONTROL.ino`:
```cpp
const unsigned long NORMAL_GREEN_TIME = 10000;  // milliseconds
```

### Change LDR Thresholds
Edit in `esp32_2_CONTROL.ino`:
```cpp
#define DARK_THRESHOLD 500
#define LIGHT_THRESHOLD 1000
```

### Change Emergency RFID Tag
Edit in `esp32_1_SENSOR.ino`:
```cpp
byte emergencyUID[4] = {0xDE, 0xAD, 0xBE, 0xEF};  // Your tag UID
```

## 📝 Notes

- ESP32 #1 sends data every 500ms
- Website refreshes every 1 second
- RFID readers use VSPI bus (shared)
- Traffic lights use 12 GPIO pins (4 lanes × 3 colors)
- LDR uses analog input (ADC)
- System supports up to 4 lanes simultaneously

## 🔮 Future Enhancements

- [ ] Upload website to ESP32 SPIFFS
- [ ] Add authentication for web dashboard
- [ ] Implement data logging to SD card
- [ ] Add mobile app support
- [ ] Create REST API documentation
- [ ] Add more traffic patterns

## 📞 Support

Check the `Guide` folder for detailed documentation:
- Hardware connection diagrams
- Pin assignment details
- Testing procedures
- System logic explanations

---

**Status:** ✅ Ready to deploy!  
**Last Updated:** 2026-06-11
