# MatterEnvironmentalStation

[![Build](https://github.com/ferreira-igor/MatterEnvironmentalStation/actions/workflows/compile-sketch.yml/badge.svg)](https://github.com/ferreira-igor/MatterEnvironmentalStation/actions/workflows/compile-sketch.yml)
![Platform](https://img.shields.io/badge/Platform-ESP32-blue)
![Framework](https://img.shields.io/badge/Framework-Arduino-green)
![Matter](https://img.shields.io/badge/Matter-Compatible-brightgreen)

This project transforms an ESP32-based board into a Matter-compatible environmental monitoring station with a 20x4 LCD display. It provides seamless integration with smart home ecosystems like Alexa, Google Home, and Apple Home through the Matter protocol, while offering real-time environmental data visualization.

Designed for smart home enthusiasts and developers, this device automatically detects and commissions itself into your Matter network, providing temperature, humidity, pressure, VOC (Volatile Organic Compounds), and motion detection without complex configuration.

The system features automatic commissioning method selection (BLE or WiFi), persistent Matter configuration, robust error recovery with automatic reboot, LED status indication for easy setup, and a 20x4 LCD display for local data visualization.

## Features

- **Matter Protocol Support**: Compatible with Matter-certified smart home hubs (Alexa, Google Home, Apple Home)
- **Multi-Sensor Environmental Monitoring**:
  - **Temperature** (BME280) - ±1°C accuracy, -40°C to +85°C range
  - **Humidity** (BME280) - ±3% accuracy, 0-100% range
  - **Pressure** (BME280) - ±1 hPa accuracy, 300-1100 hPa range
  - **VOC Index** (SGP40) - Relative air quality measurement (0-500 scale)
  - **Motion Detection** (HW-MS03 PIR) - 7m range, 120° detection angle
- **20x4 LCD Display**: Shows sensor readings and system status with custom icons
- **Automatic Commissioning**: Smart method selection based on hardware capabilities
  - **BLE Commissioning** (ESP32-C3): Standard Matter commissioning via BLE - WiFi credentials provided by the commissioner app
  - **WiFi Commissioning** (Fallback): Captive portal for manual WiFi configuration when BLE is unavailable
- **Smart Reporting**: Configurable thresholds (1.0°C temperature, 3.0% humidity, 1.0 hPa pressure) to minimize network traffic
- **Automatic Recovery**: Detects and handles Matter disconnections, reboots after 60 seconds of lost connectivity
- **LED Status Indication**: Clear visual feedback - LED ON means ready for commissioning, LED OFF means commissioned
- **Real-Time Display**: LCD shows current readings, motion status, and air quality classification
- **Automatic Reconnection**: WiFi watchdog for reliable connectivity
- **Error Handling**: Graceful failure recovery with automatic restarts

## Hardware

### Required Components

- ESP32 Development Board (Tested on WeAct Studio ESP32C3, compatible with most ESP32 boards)
- BME280 Temperature, Humidity and Pressure Sensor (I2C address 0x76)
- SGP40 VOC Index Sensor (I2C address 0x59)
- HW-MS03 PIR Motion Sensor (Digital output, 3.3V compatible)
- 20x4 LCD Display (HD44780-compatible, parallel interface)
- USB Cable for programming and power
- Connecting wires

### Supported Boards

The code is tested on the WeAct Studio ESP32C3 but should work on any ESP32-based board with the following:
- Built-in LED (configurable via LED_BUILTIN)
- I2C pins (configurable via pin_i2c_scl and pin_i2c_sda)
- Parallel GPIO pins for LCD (configurable)
- BLE support (for automatic commissioning) or fallback to WiFi commissioning

### Sensor Compatibility

The BME280 and SGP40 sensors are recommended and tested. Other sensors may work with minor code modifications:
- **BME280**: Recommended (temperature, humidity, pressure)
- **BME680**: Alternative (temperature, humidity, pressure, gas resistance)
- **SGP30**: Alternative VOC sensor (would require code changes)
- **AHT10/AHT20**: Alternative humidity/temperature sensor (would require code changes)
- **HC-SR501**: Compatible PIR motion sensor (5V logic level - requires level shifting)

## Wiring

The project requires multiple connections for sensors and display:

### I2C Sensors (BME280 and SGP40)

| Component | ESP32 Pin | Description |
|-----------|-----------|-------------|
| BME280 VCC | 3.3V | Power supply |
| BME280 GND | GND | Ground |
| BME280 SCL | GPIO 0 | I2C Clock (WeAct Studio ESP32C3) |
| BME280 SDA | GPIO 1 | I2C Data (WeAct Studio ESP32C3) |
| SGP40 VCC | 3.3V | Power supply |
| SGP40 GND | GND | Ground |
| SGP40 SCL | GPIO 0 | I2C Clock (shared with BME280) |
| SGP40 SDA | GPIO 1 | I2C Data (shared with BME280) |

### LCD Display (Parallel 4-bit Mode)

| LCD Pin | ESP32 Pin | Description |
|---------|-----------|-------------|
| VSS | GND | Ground |
| VDD | 5V or 3.3V | Power supply (check LCD specs) |
| V0 | Potentiometer | Contrast adjustment (10kΩ) |
| RS | GPIO 7 | Register Select |
| RW | GND | Read/Write (tied to GND for write-only) |
| EN | GPIO 6 | Enable |
| D4 | GPIO 5 | Data line 4 |
| D5 | GPIO 4 | Data line 5 |
| D6 | GPIO 3 | Data line 6 |
| D7 | GPIO 2 | Data line 7 |
| BLA | 5V or 3.3V | Backlight power |
| BLK | GND | Backlight ground |

### HW-MS03 PIR Motion Sensor

| Component | ESP32 Pin | Description |
|-----------|-----------|-------------|
| HW-MS03 VCC | 5.0V | Power supply |
| HW-MS03 GND | GND | Ground |
| HW-MS03 OUT | GPIO 10 | Digital output (HIGH = motion) |

### Built-in LED

| Component | ESP32 Pin | Description |
|-----------|-----------|-------------|
| Built-in LED | LED_BUILTIN | Status indicator (active LOW) |

**Note**: For other ESP32 boards, adjust the pins in the code:
- I2C pins: Change `pin_i2c_scl` and `pin_i2c_sda` (default: 0, 1)
- LCD pins: Change `pin_lcd_rs`, `pin_lcd_en`, `pin_lcd_d4`-`pin_lcd_d7` (default: 7, 6, 5, 4, 3, 2)
- Motion sensor: Change `pin_motion` (default: 10)

## Flashing

### Method 1: Pre-compiled Binary

1. Download the latest binary from the Releases page

2. Install esptool:
   ```bash
   pipx install esptool
   ```

3. Using esptool:
   ```bash
   esptool --port /dev/ttyUSB0 erase-flash
   esptool --port /dev/ttyUSB0 write-flash 0x0 MatterEnvironmentalStation.ino.merged.bin
   ```

   Replace /dev/ttyUSB0 with your actual serial port.

### Method 2: Using Arduino IDE

1. **Install ESP32 Core**:
   - Open Arduino IDE
   - Go to File > Preferences
   - Add https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json to Additional Boards Manager URLs
   - Go to Tools > Board > Boards Manager
   - Search for ESP32 and install esp32 by Espressif Systems (v3.3.11 or later)

2. **Install Required Libraries**:
   - Open Sketch > Include Library > Manage Libraries
   - Install the following libraries:
     - Adafruit BME280 Library by Adafruit (v2.3.0)
     - Adafruit SGP40 by Adafruit (v1.1.4)
     - LiquidCrystal by Arduino (v1.0.7) - built-in
     - Matter by Arduino (v1.2.5 or later)
     - WiFiManager by tzapu (v2.0.17) - only required when BLE commissioning is disabled

3. **Configure and Upload**:
   - Open MatterEnvironmentalStation.ino in Arduino IDE
   - Select your ESP32 board: Tools > Board > ESP32 Arduino > [Your Board Model]
   - Select the correct port: Tools > Port > [Your USB Port]
   - Select "Erase All Flash Before Sketch Upload: Enabled"
   - Select "Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)"
   - Click Upload (arrow icon) to compile and flash

4. **Monitor Serial Output**:
   - Open Tools > Serial Monitor
   - Set baud rate to 115200
   - Observe startup logs and sensor readings

## Startup

### First Boot and Commissioning

The commissioning method depends on your ESP32 board and the CONFIG_ENABLE_CHIPOBLE flag:

#### Method 1: BLE Commissioning (Recommended - ESP32-C3)

1. **Power the Device**: Connect the ESP32 via USB or power supply
2. **LED Behavior**:
   - LED ON (LOW): Device is NOT commissioned - ready for setup
   - LED OFF (HIGH): Device IS commissioned - operating normally
3. **LCD Display**: Shows commissioning status and pairing code
4. **Commission via Smart Home App**:
   - Open your Matter-compatible app (Alexa, Google Home, Apple Home)
   - Add a new device and scan the QR code displayed in the serial monitor
   - Or enter the manual pairing code shown on the LCD and serial output
   - The app will provide WiFi credentials via BLE
   - Wait for commissioning to complete
   - The LED will turn OFF (HIGH) once commissioned
   - LCD will change to "Room Stats" display mode

#### Method 2: WiFi Commissioning (Fallback - no BLE)

1. **Power the Device**: Connect the ESP32 via USB or power supply
2. **Connect to Captive Portal**:
   - The device creates a WiFi access point (usually named MatterEnvironmental or similar)
   - Connect your phone or computer to this network
3. **Configure WiFi**:
   - Open a web browser and navigate to the captive portal (usually 192.168.4.1)
   - Enter your WiFi credentials
   - Device will connect to your WiFi network
4. **Commission via Matter**:
   - Use your Matter-compatible app to add the device
   - Scan the QR code or enter the manual pairing code shown on the LCD and serial monitor
   - The device will be commissioned to your Matter hub

### LCD Display Information

After startup and commissioning, the 20x4 LCD display shows:

**Row 0 (Status Line)**:
- Left icon: Commissioned (✔️) or Decommissioned (❌) status
- Center text: "Pin: XXXXXXXX" (pairing code) when uncommissioned, or "Room Stats" when commissioned
- Right icon: Connected (✔️) or Disconnected (❌) status

**Row 1 (Motion Status)**:
- Displays "Motion Detected!" when HW-MS03 PIR sensor triggers
- Displays "====================" when no motion detected

**Row 2 (Sensor Readings)**:
- Temperature (🌡️) with custom icon
- Humidity (💧) with custom icon  
- Pressure (↕️) with custom icon

**Row 3 (Air Quality)**:
- Based on VOC index from SGP40:
  - **Excellent** (≤ 100): "Air is Excellent"
  - **Good** (101-150): "Air is Good"
  - **Moderate** (151-250): "Air is Moderate"
  - **Poor** (251-400): "Air is Poor"
  - **Unhealthy** (> 400): "Air is Unhealthy"

### Commissioning Information

After startup, the serial monitor will display:
- Manual pairing code (numeric code for manual entry)
- QR code URL (for scanning with smart home apps)

The LCD also displays the pairing code on the top line when uncommissioned.

### LED Status Indicator

| LED State | Pin State | Meaning |
|-----------|-----------|---------|
| ON (LOW) | 0 | Device NOT commissioned - ready for setup |
| OFF (HIGH) | 1 | Device IS commissioned - operating normally |

## Configuration

### Sensor Reporting Thresholds

The device uses smart reporting to minimize network traffic:

- **Temperature**: Reports when changes exceed 1.0°C (BME280 accuracy)
- **Humidity**: Reports when changes exceed 3.0% (BME280 accuracy)
- **Pressure**: Reports when changes exceed 1.0 hPa (BME280 accuracy)
- **Motion**: Reports immediately on any state change

These thresholds can be adjusted in the code:
- `temperature_diff`: Adjust as needed (default: 1.0f)
- `humidity_diff`: Adjust as needed (default: 3.0f)
- `pressure_diff`: Adjust as needed (default: 1.0f)

### Timing Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| Sensors Read Interval | 1 second | How often sensor is read |
| Display Update Interval | 5 seconds | How often LCD is updated |
| Matter Check Interval | 15 seconds | How often connection is verified |
| Matter Timeout | 3 minutes | Max disconnection time before reboot |
| WiFi Manager Timeout | 3 minutes | Captive portal timeout (WiFi method) |

### LCD Custom Icons

The firmware creates custom LCD icons for a more intuitive display:

| Icon | Description | CGRAM Index |
|------|-------------|-------------|
| 🌡️ | Temperature symbol | 0 |
| 💧 | Humidity symbol | 1 |
| ↕️ | Pressure symbol | 2 |
| ❌ | Decommissioned status | 3 |
| ✔️ | Commissioned status | 4 |
| ❌ | Disconnected status | 5 |
| ✔️ | Connected status | 6 |

## Notes

### Important Considerations

- **Matter Compatibility**: Ensure your smart home hub supports Matter (most modern hubs do)
- **Network Requirements**: Device must be on the same network as your Matter hub
- **BLE Commissioning**: Requires BLE support on the ESP32 (ESP32-C3 has built-in BLE)
- **Power Requirements**: ESP32 boards typically require 5V via USB or 3.3V from a regulated power supply
- **Sensor Placement**: For accurate readings, place the sensors away from heat sources and in open air
- **LCD Contrast**: May need adjustment via potentiometer for optimal readability
- **SGP40 Warm-Up**: VOC sensor requires stabilization time after power-on

### Performance and Limitations

- **Reading Frequency**: Sensors are read every 1 second - sufficient for environmental monitoring
- **Reporting Frequency**: Reports are sent only when thresholds are exceeded, reducing network traffic
- **Connection Recovery**: Automatic reboot after 60 seconds of disconnection ensures reliability
- **Memory Usage**: Matter library requires significant flash - use appropriate partition scheme
- **LCD Update Rate**: 5-second interval reduces display flicker and microcontroller load

### Security Considerations

- **Matter Security**: Uses Matter's built-in security and encryption
- **WiFi Credentials**: Stored in ESP32's non-volatile storage
- **BLE Commissioning**: Secure pairing using Matter's standard commissioning process
- **WiFi Manager**: Only active during setup, not during normal operation

### Troubleshooting

| Issue | Solution |
|-------|----------|
| BME280 not initializing | Check I2C wiring; verify address (0x76); ensure 3.3V power |
| SGP40 not initializing | Check I2C wiring; verify address (0x59); allow warm-up time |
| LCD not displaying | Check contrast potentiometer; verify wiring; check power supply |
| LCD shows garbled characters | Check LCD data pins (D4-D7); verify Enable and Register Select pins |
| LED stays ON continuously | Device is not commissioned - complete commissioning process |
| Commissioning fails | Verify Matter hub supports the device; check network connectivity |
| No sensor readings | Check I2C wiring; verify I2C addresses; check library versions |
| Device doesn't appear in Matter app | Ensure device is on the same network; reboot device and try again |
| Frequent disconnections | Check WiFi signal strength; adjust device placement |
| Motion sensor not detecting | Check wiring; verify pin 10; adjust sensitivity potentiometer |
| Serial output shows no QR code | Ensure partition scheme has enough space for Matter library |
| LCD backlight not working | Check BLA/BLK connections; verify power supply |

### VOC Index Guide

The SGP40 VOC index provides relative air quality information:

| VOC Index | Air Quality | Description |
|-----------|-------------|-------------|
| 0-100 | Excellent | Very low volatile organic compounds |
| 101-150 | Good | Acceptable air quality |
| 151-250 | Moderate | Some VOCs present |
| 251-400 | Poor | Elevated VOCs - consider ventilation |
| > 400 | Unhealthy | High VOCs - ventilate immediately |

Note that VOC index is relative and requires baseline calibration for absolute measurements.