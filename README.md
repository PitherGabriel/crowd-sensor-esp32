# Crowd Density Sensor - ESP32 Implementation

WiFi/Bluetooth sniffing sensor for real-time crowd density estimation at festivals and events.

## Features

- **WiFi Sniffing**: Detects devices via promiscuous mode packet capture
- **BLE Scanning**: Detects Bluetooth Low Energy devices
- **Privacy-First**: All MAC addresses immediately hashed with SHA256
- **Real-Time**: MQTT transmission every 30 seconds
- **Low Power**: 12-24 hours battery life, 3-5 days with deep sleep
- **Accurate**: Target ±20% accuracy after calibration

## Hardware Requirements

- ESP32 development board (ESP32-WROOM-32 or ESP32-S3)
- USB cable for programming
- Optional: LiPo battery (5000-10000mAh), solar panel, 4G module

## Software Requirements

- ESP-IDF v5.1 or later
- Python 3.7+

## Quick Start

### 1. Install ESP-IDF

```bash
# Install prerequisites
sudo apt-get install git wget flex bison gperf python3 python3-pip \
     python3-setuptools cmake ninja-build ccache libffi-dev libssl-dev \
     dfu-util libusb-1.0-0

# Clone ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v5.1

# Install tools
./install.sh esp32

# Set up environment (add to ~/.bashrc)
. $HOME/esp/esp-idf/export.sh
```

### 2. Configure the Project

Edit `main/config.h` to set:
- `SENSOR_ID`: Unique identifier for this sensor
- `SENSOR_LOCATION`: Zone name
- `MQTT_BROKER`: Your MQTT broker URL
- `MAC_HASH_SALT`: Random string for privacy

Edit `main/main.c` to set:
- `WIFI_SSID`: WiFi network name (for internet connectivity)
- `WIFI_PASSWORD`: WiFi password

### 3. Build and Flash

```bash
cd ~/crowd-sensor-esp32

# Build
idf.py build

# Flash to ESP32 (adjust port as needed)
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output
idf.py -p /dev/ttyUSB0 monitor

# Exit monitor: Ctrl+]
```

### 4. Expected Output

```
I (123) Main: === Crowd Density Sensor Starting ===
I (456) WiFi_Sniffer: WiFi sniffer initialized
I (789) BLE_Scanner: BLE scanner initialized
I (1234) WiFi_Sniffer: New device detected: a1b2... RSSI: -65 dBm
I (1567) BLE_Scanner: New BLE device: c3d4... RSSI: -70 dBm
I (2345) Density_Estimator: Estimate: WiFi=5, BLE=3, Unique=5, People=7
I (2346) MQTT_Client: Published: 7 people (msg_id=1)
```

## Configuration Options

### WiFi Sniffing

- `WIFI_CHANNELS`: Channels to scan (default: 1, 6, 11)
- `WIFI_CHANNEL_HOP_INTERVAL`: Time per channel in ms (default: 2000)
- `MIN_RSSI`: Filter threshold in dBm (default: -85)

### Crowd Estimation

- `CORRECTION_FACTOR`: Multiplier to account for devices without WiFi/BT (default: 1.35)
- `MAC_TTL_SECONDS`: Time before device expires from cache (default: 900 = 15 min)

### MQTT Publishing

- `PUBLISH_INTERVAL`: Seconds between transmissions (default: 30)
- `MQTT_TOPIC`: Topic to publish to (default: "crowd/density")

## Calibration

After field testing, adjust `CORRECTION_FACTOR` in `config.h`:

```
correction_factor = actual_people_count / detected_devices
```

Example: If you detect 15 devices but count 20 people:
```
CORRECTION_FACTOR = 20 / 15 = 1.33
```

## MQTT Payload Format

```json
{
  "sensor_id": "sensor-001",
  "location": "main_stage_north",
  "coordinates": {
    "lat": 56.1629,
    "lon": 10.2039
  },
  "wifi_devices": 5,
  "ble_devices": 3,
  "unique_devices": 5,
  "estimated_people": 7,
  "correction_factor": 1.35,
  "timestamp": 1680123456,
  "density_level": "comfortable"
}
```

## Troubleshooting

### Build Errors

**Error**: `ESP-IDF not found`
```bash
. $HOME/esp/esp-idf/export.sh
```

**Error**: `Bluetooth components not found`
```bash
idf.py menuconfig
# Component config → Bluetooth → Enable
```

### Runtime Issues

**No WiFi devices detected**
- Check ESP32 has WiFi enabled
- Verify promiscuous mode is working: `esp_wifi_set_promiscuous(true)`
- Ensure people nearby have WiFi enabled on phones

**No BLE devices detected**
- Check Bluetooth is enabled in menuconfig
- Verify people have Bluetooth enabled
- Check RSSI threshold (`MIN_RSSI`)

**MQTT not connecting**
- Verify WiFi credentials in `main.c`
- Check MQTT broker URL and port
- Test with `mosquitto_pub/sub` on another machine

## Power Optimization

For multi-day battery operation, enable deep sleep in `config.h`:

```c
#define ENABLE_DEEP_SLEEP true
#define DEEP_SLEEP_INTERVAL 60  // Wake every 60 seconds
```

This reduces average power to ~20mA, extending battery life to 3-5 days.

## Next Steps

1. **Test in controlled environment** (office with known number of people)
2. **Calibrate correction factor** based on test results
3. **Field test** at small outdoor event
4. **Validate accuracy** (target: ±20-25% error)
5. **Prepare demo** for Smukfest pitch

## Project Structure

```
crowd-sensor-esp32/
├── main/
│   ├── config.h                 # Configuration constants
│   ├── wifi_sniffer.h/c         # WiFi promiscuous mode sniffer
│   ├── bt_scanner.h/c           # BLE scanner
│   ├── density_estimator.h/c    # Crowd estimation algorithm
│   ├── mqtt_client_crowd.h/c    # MQTT client
│   ├── main.c                   # Main application
│   └── CMakeLists.txt           # Component build config
├── CMakeLists.txt               # Project build config
└── README.md                    # This file
```

## Privacy Compliance

✅ MAC addresses immediately hashed with SHA256
✅ No raw MAC addresses stored or transmitted
✅ Data retention: 15 minutes (configurable)
✅ GDPR compliant (consult legal for confirmation)

## License

Copyright © 2025 Inti Crowd Systems
