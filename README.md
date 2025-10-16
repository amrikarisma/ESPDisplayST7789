# ESP32 Display ST7789 for Speeduino & rusEFI (Haltech Canbus)

ESP32-based display system for Speeduino ECU with ST7789 TFT display (320x170). Supports both CAN bus and Serial communication protocols.

## Hardware Requirements

### Main Components
- **ESP32 DOIT DEV KIT v1** 
- **ST7789 TFT Display** (320x170 pixels)
- **CAN Transceiver Module** (for CAN communication)
- **MCP2515/TJA1050** or similar CAN transceiver

## Pin Configuration

### TFT Display Connections (ST7789 - 320x170)

| GPIO | TFT   | Description         |
| ---: | :---- | :------------------ |
| 23   | SDA   | MOSI Hardware SPI   |
| 18   | SCK   | CLK  Hardware SPI   |
| 15   | CS    | CS                  |
|  2   | DC    | DC                  |
|  4   | RES   | Reset               |
| 21   | BLK   | 3.3V ( or PWM-Pin ) |
|      | VCC   | 3.3V                |
|      | GND   | GND                 |

### CAN Bus Communication

| GPIO | Function | Description           |
| ---: | :------- | :-------------------- |
| 17   | CAN_RX   | CAN Receive           |
| 16   | CAN_TX   | CAN Transmit          |
|      | VCC      | 3.3V/5V               |
|      | GND      | GND                   |

**CAN Speed:** Auto-detection (1000000, 500000, 250000, 125000 bps)

### Serial Communication (Alternative)

| GPIO | Function | Description           |
| ---: | :------- | :-------------------- |
| 26   | RX       | Serial Receive        |
| 25   | TX       | Serial Transmit       |
|      | GND      | GND                   |

**Serial Settings:** 115200 baud, 8N1

### Backlight Control

| GPIO | Function | Description           |
| ---: | :------- | :-------------------- |
| 32   | BL_PWM   | Backlight PWM Control |

## PlatformIO Environment

### Environment Configuration
```ini
[env:esp32doit-devkit-v1]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino
monitor_speed = 115200
upload_speed = 460800
```

### Required Libraries
- **TFT_eSPI** (Custom fork): https://github.com/amrikarisma/TFT_eSPI.git
- **ESP32_CAN**: https://github.com/amrikarisma/esp32_can.git  
- **can_common**: https://github.com/amrikarisma/can_common.git

## Features

### Display System
- **9-Panel Dynamic Layout** (4 top + 5 bottom)
- **6 Indicator Lights** (SYNC, FAN, REV, LCH, AC, DFCO)
- **Web Configuration Interface** (192.168.4.1)
- **Speeduino Logo Splash Screen**

### Supported Data Sources
- RPM, MAP, TPS, CLT, IAT
- AFR, Battery Voltage, Fuel Pressure
- Ignition Advance, VSS
- Engine Status Indicators

### Communication Protocols
- **CAN Bus** (Haltech protocol compatible)
- **Serial** (Speeduino TS format)
- **WiFi AP** for configuration

## Installation

### Hardware Setup
1. Connect ST7789 display according to pin table
2. Connect CAN transceiver (for CAN mode) OR Serial connection (for Serial mode)
3. Connect power (3.3V/5V depending on modules)

### Software Setup
1. Install PlatformIO IDE
2. Clone this repository
3. Open project in PlatformIO
4. Build and upload to ESP32

```bash
# Build project
pio run -e esp32doit-devkit-v1

# Upload firmware
pio run -e esp32doit-devkit-v1 -t upload

# Monitor serial output
pio device monitor -p /dev/cu.wchusbserial1120
```

## Configuration

### Web Interface
1. Connect to WiFi AP: **"MAZDUINO_Display"** (Password: **"12345678"**)
2. Navigate to: **http://192.168.4.1**
3. Configure data sources and layout
4. Save settings to EEPROM

### Display Layout
- **Top Row (4 panels):** CLT, IAT, AFR, BAT
- **Bottom Row (5 panels):** RPM, FP, TPS, MAP, ADV
- **Indicators (6 max):** SYNC, FAN, REV, LCH, AC, DFCO

## CAN Protocol Support

### Supported CAN IDs (Haltech Format)
- `0x360`: RPM, MAP, TPS
- `0x361`: Fuel Pressure
- `0x362`: Ignition Advance
- `0x368`: AFR (Lambda)
- `0x369`: Trigger Error Count
- `0x370`: Vehicle Speed
- `0x372`: Battery Voltage
- `0x3E0`: CLT, IAT (Temperature)
- `0x3E4`: Engine Status Indicators

## Troubleshooting

### Common Issues
1. **No CAN data:** Check wiring, CAN speed, termination resistors
2. **Display not working:** Verify SPI connections and power supply
3. **WiFi not connecting:** Reset EEPROM, check AP mode
4. **Serial communication:** Verify baud rate and pin connections

### Debug Output
Enable serial monitoring at 115200 baud for diagnostic information.

## References

- [TFT_eSPI Library Issues](https://github.com/Bodmer/TFT_eSPI/issues/3384#issuecomment-2211470238)
- [ST7789 Display Examples](https://github.com/mboehmerm/Three-IPS-Displays-with-ST7789-170x320-240x280-240x320/tree/main/ESP32_C3)
- [Mazduino](https://www.mazduino.com/)

## License

Open source project - see LICENSE file for details.
