# Matter Environmental Station

A Matter-compatible environmental monitoring station based on the **WeAct Studio ESP32-C3**, combining multiple environmental sensors, a PIR motion detector, and a 20x4 HD44780-compatible LCD.

The station measures:

- 🌡️ Temperature
- 💧 Relative humidity
- 🌡️ Atmospheric pressure
- 🫧 VOC (Volatile Organic Compounds) index
- 🚶 Motion / occupancy

Temperature, humidity, pressure, and motion are exposed through standard Matter endpoints for integration with compatible smart-home platforms. The VOC index is currently used locally to classify air quality on the LCD.

## Features

- Matter support for smart-home integration
- Temperature measurement using BME280
- Relative humidity measurement using BME280
- Atmospheric pressure measurement using BME280
- VOC index measurement using SGP40
- Motion detection using HW-MS03 PIR
- 20x4 parallel HD44780 LCD
- Custom LCD icons stored in CGRAM
- Human-readable air-quality classification
- Matter commissioning status on the LCD
- Matter connection status on the LCD
- Matter pairing code displayed while the device is not commissioned
- BLE Matter commissioning on BLE-capable ESP32 configurations
- WiFiManager fallback when BLE commissioning is disabled
- Change-based Matter reporting to reduce unnecessary updates
- Immediate Matter occupancy updates when motion state changes
- Automatic Matter disconnection recovery
- Built-in LED commissioning/status indicator
- Non-blocking periodic tasks using `millis()`

---

## Hardware

The reference hardware is:

| Component | Details |
|---|---|
| Microcontroller | WeAct Studio ESP32-C3 |
| Environmental sensor | BME280 |
| VOC sensor | SGP40 |
| Motion sensor | HW-MS03 PIR |
| Display | 20x4 HD44780-compatible LCD |
| LCD interface | Parallel 4-bit |
| I²C | GPIO 0 / GPIO 1 |

### Sensors

#### BME280

The BME280 provides:

- Temperature
- Relative humidity
- Atmospheric pressure

Configured I²C address:

```text
0x76
```

The BME280 address can also be `0x77` on some modules, depending on the state of the `ADDR` pin.

#### SGP40

The SGP40 provides the VOC index used for the local air-quality indication.

Default I²C address:

```text
0x59
```

The SGP40 measurement is temperature/humidity compensated using the current BME280 readings.

#### HW-MS03

The PIR sensor is connected to GPIO 10.

| Signal | ESP32-C3 |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| OUT | GPIO 10 |

The firmware configures the input as:

```cpp
pinMode(pin_motion, INPUT_PULLDOWN);
```

Sensor output:

```text
HIGH = Motion detected
LOW  = No motion
```

The sensor has an approximate warm-up time of 2 seconds after power-up and provides adjustable sensitivity/time-delay controls on typical HW-MS03 modules.

---

# Pinout

## I²C

The BME280 and SGP40 share the same I²C bus.

| ESP32-C3 | Function |
|---:|---|
| GPIO 0 | SCL |
| GPIO 1 | SDA |

```text
ESP32-C3
   │
   ├── GPIO 0 ───── SCL ──┬── BME280
   │                      └── SGP40
   │
   └── GPIO 1 ───── SDA ──┬── BME280
                          └── SGP40
```

## LCD

The LCD operates in 4-bit parallel mode.

| LCD signal | ESP32-C3 |
|---|---:|
| RS | GPIO 7 |
| EN | GPIO 6 |
| D4 | GPIO 5 |
| D5 | GPIO 4 |
| D6 | GPIO 3 |
| D7 | GPIO 2 |

LCD `D0-D3` are not connected.

Typical power connections:

```text
LCD VSS  → GND
LCD VDD  → 5V
LCD VO   → Contrast potentiometer
LCD RS   → GPIO 7
LCD EN   → GPIO 6
LCD D4   → GPIO 5
LCD D5   → GPIO 4
LCD D6   → GPIO 3
LCD D7   → GPIO 2
```

> Check the voltage requirements of your particular LCD module. The LCD signal levels must be compatible with the ESP32-C3 GPIOs.

## Built-in LED

The firmware uses:

```cpp
LED_BUILTIN
```

and assumes an active-low LED:

```text
LOW  = LED ON
HIGH = LED OFF
```

---

# LCD interface

The 20x4 LCD is used as a local dashboard.

The four lines are organized as follows:

```text
┌────────────────────┐
│ Matter / Pairing   │
│ Motion status      │
│ Temperature/H/R    │
│ Air quality        │
└────────────────────┘
```

## Row 1 — Matter status

When the device has not been commissioned:

```text
[icon] Pin: 12345678 [icon]
```

The manual Matter pairing code is displayed.

After commissioning:

```text
[icon]    Room Stats    [icon]
```

The icons at the sides indicate commissioning and Matter connection status.

## Row 2 — Motion

When motion is detected:

```text
  Motion Detected!
```

When there is no motion:

```text
====================
```

## Row 3 — Environmental measurements

The display shows rounded integer values:

```text
[°C] 25C  [%] 65%  [hPa] 1013hPa
```

The firmware uses `lround()` for the displayed values.

The underlying sensor values remain floating-point values.

## Row 4 — Air quality

The VOC index is converted into a human-readable message:

| VOC Index | LCD message |
|---:|---|
| 0–100 | Air is Excellent |
| 101–150 | Air is Good |
| 151–250 | Air is Moderate |
| 251–400 | Air is Poor |
| >400 | Air is Unhealthy |

The numeric VOC index itself is currently not displayed on the LCD.

---

# LCD custom characters

The LCD uses CGRAM custom characters for its icons.

The firmware creates seven custom characters:

| Index | Function |
|---:|---|
| `0` | Temperature |
| `1` | Humidity |
| `2` | Pressure |
| `3` | Decommissioned |
| `4` | Commissioned |
| `5` | Disconnected |
| `6` | Connected |

They are created during initialization using:

```cpp
lcd.createChar(...)
```

and displayed with:

```cpp
lcd.write(...)
```

Because a standard HD44780 LCD only provides eight custom-character slots, the project uses seven of them.

---

# Matter endpoints

The station creates four Matter endpoints:

```cpp
MatterTemperatureSensor temperatureSensor;
MatterHumiditySensor humiditySensor;
MatterPressureSensor pressureSensor;
MatterOccupancySensor occupancySensor;
```

They represent:

```text
Matter Environmental Station
├── Temperature
├── Humidity
├── Pressure
└── Occupancy
```

The VOC index is **not currently exposed through Matter**. It is used locally by the firmware to generate the LCD air-quality description.

## Why VOC is not exposed

The current implementation does not create a dedicated Matter VOC/air-quality endpoint.

Adding such an endpoint would depend on the Matter clusters supported by the ESP32 Arduino Matter implementation and by the target smart-home ecosystems.

---

# Matter reporting strategy

The firmware reads all sensors every second, but does not send every measurement to Matter.

Instead, it compares the current value with the last value reported.

This significantly reduces unnecessary Matter updates caused by normal sensor fluctuations.

## Temperature

Matter is updated when the temperature changes by at least:

```text
1.0 °C
```

Configured as:

```cpp
const float temperature_diff = 1.0f;
```

## Humidity

Matter is updated when humidity changes by at least:

```text
3.0 %
```

Configured as:

```cpp
const float humidity_diff = 3.0f;
```

## Pressure

Matter is updated when pressure changes by at least:

```text
1.0 hPa
```

Configured as:

```cpp
const float pressure_diff = 1.0f;
```

## Occupancy

Motion is handled differently.

Any change in state is reported:

```text
No motion → Motion
Motion → No motion
```

This means occupancy does not use a numeric threshold.

---

# Sensor sampling and Matter updates

The firmware separates sensor sampling from Matter reporting.

Every second:

```text
Read BME280
Read SGP40
Read PIR
      │
      ▼
Compare against last Matter values
      │
      ├── Significant change → Update Matter
      │
      └── Small change → Do nothing
```

This allows the local LCD and air-quality calculation to receive frequent measurements while avoiding excessive Matter traffic.

---

# SGP40 VOC measurement

The SGP40 is read using:

```cpp
sgp.measureVocIndex(current_temperature, current_humidity);
```

The current BME280 temperature and humidity measurements are passed to the SGP40 library for environmental compensation.

The resulting VOC index is stored in:

```cpp
int32_t current_voc;
```

The value is primarily used for the LCD air-quality classification.

## Important interpretation

The VOC Index is a **relative air-quality indicator**, not a direct concentration measurement in ppm.

Higher values indicate a higher relative VOC condition according to the SGP40's index algorithm.

The thresholds used by this project are intended as a simple user-facing classification rather than a certified air-quality standard.

---

# Matter commissioning

The project supports two commissioning approaches depending on the ESP32 build configuration.

## BLE commissioning

When:

```cpp
CONFIG_ENABLE_CHIPOBLE
```

is enabled, Matter commissioning uses BLE.

The Matter controller supplies the Wi-Fi credentials during commissioning.

Typical flow:

```text
Power on
   │
   ▼
Matter device starts
   │
   ▼
Open Alexa / Google Home / Apple Home
   │
   ▼
Add Matter device
   │
   ▼
Scan QR code or enter pairing code
   │
   ▼
Commissioner provides Wi-Fi credentials
   │
   ▼
Device joins Wi-Fi
   │
   ▼
Matter endpoints become available
```

This is the preferred commissioning method for the ESP32-C3.

## WiFiManager fallback

When BLE Matter commissioning is disabled:

```cpp
#if !CONFIG_ENABLE_CHIPOBLE
```

the firmware includes WiFiManager.

The captive portal uses:

```text
Configuration portal timeout: 180 seconds
Wi-Fi connection timeout: 30 seconds
```

If the device cannot connect to Wi-Fi through the portal, it restarts.

This mode is useful for:

- ESP32 boards without BLE support;
- development;
- debugging;
- testing alternative commissioning flows.

---

# Pairing information

When the device has not been commissioned, the firmware prints the Matter pairing information to the Serial Monitor:

```text
Matter Node is not commissioned yet.
Initiate the device discovery in your Matter environment.
Commission it to your Matter hub with the manual pairing code or QR code
Manual pairing code: XXXXXXXX
QR code URL: ...
```

The manual pairing code is also shown on the LCD.

---

# LED status

The built-in LED provides a simple Matter commissioning indicator.

| State | LED |
|---|---|
| Device not commissioned | ON |
| Device commissioned | OFF |

The LED is controlled by `checkMatter()` every 15 seconds.

---

# Matter connection monitoring and recovery

The firmware checks Matter status every:

```text
15 seconds
```

using:

```cpp
Matter.isDeviceCommissioned()
Matter.isDeviceConnected()
```

The firmware distinguishes between:

### Not commissioned

```text
Commissioned = false
```

The LED remains ON and the pairing information is available.

### Commissioned and connected

```text
Commissioned = true
Connected = true
```

Normal operation.

### Commissioned but disconnected

```text
Commissioned = true
Connected = false
```

The firmware starts a disconnection counter.

If the device remains disconnected for approximately:

```text
60 seconds
```

it performs:

```cpp
Matter.decommission();
```

then waits one second and restarts the ESP32.

## Important consequence

This recovery mechanism deliberately removes the Matter commissioning state.

After such a recovery, the device may need to be commissioned again.

This is an aggressive recovery strategy intended for cases where the Matter connection becomes stuck rather than simply experiencing a short network interruption.

---

# Timing architecture

The firmware uses `millis()` to schedule independent periodic operations.

No recurring `delay()` is used in the main loop.

| Task | Interval |
|---|---:|
| Sensor reading | 1 second |
| LCD update | 5 seconds |
| Matter connection check | 15 seconds |
| Matter disconnection timeout | 60 seconds |

## Main loop

Conceptually:

```text
loop()
 │
 ├── Every 1 s
 │     ├── Read sensors
 │     └── Update Matter if necessary
 │
 ├── Every 5 s
 │     └── Refresh LCD
 │
 └── Every 15 s
       └── Check Matter connection
```

This approach keeps the firmware responsive and avoids unnecessarily blocking the Matter stack.

---

# Startup sequence

The firmware initializes the system in the following order:

```text
1. Configure GPIOs
2. Start Serial
3. Initialize LCD
4. Create LCD custom characters
5. Initialize I²C
6. Initialize BME280
7. Initialize SGP40
8. Configure Wi-Fi if WiFiManager mode is active
9. Initialize Matter endpoints
10. Start Matter
11. Display commissioning information
12. Enter main loop
```

---

# Error handling

## BME280 initialization

If the BME280 cannot be initialized:

```text
Error initializing BME280 sensor! Check your wiring!
```

is printed.

The firmware currently continues execution.

## SGP40 initialization

If the SGP40 cannot be initialized:

```text
Error initializing SGP40 sensor! Check your wiring!
```

is printed.

The firmware continues execution.

## Sensor read errors

Invalid BME280 values are detected using `isnan()`.

Examples:

```text
Error reading temperature!
Error reading humidity!
Error reading pressure!
```

The previous valid value is retained.

The same validation pattern is used for the VOC reading.

> If a sensor fails during startup, the current firmware does not enter a dedicated sensor-error state or automatically reinitialize the sensor. This is a possible future improvement.

---

# Software requirements

The project was developed with:

- Arduino IDE `2.3.10`
- ESP32 Arduino Core `3.3.11`

## Required libraries

| Library | Referenced version | Purpose |
|---|---:|---|
| Adafruit BME280 Library | `2.3.0` | Temperature, humidity and pressure |
| Adafruit SGP40 | `1.1.4` | VOC index |
| LiquidCrystal | Arduino library | HD44780 LCD |
| Matter | ESP32 Arduino Core | Matter protocol |
| WiFiManager | `2.0.17` | Wi-Fi configuration fallback |

WiFiManager is only compiled when BLE commissioning is disabled.

---

# Installation

## 1. Install Arduino IDE

Install Arduino IDE 2.x.

## 2. Install ESP32 support

Install the ESP32 Arduino Core and select the appropriate ESP32-C3 board.

The reference board is:

```text
WeAct Studio ESP32-C3
```

## 3. Install libraries

Install:

- Adafruit BME280 Library
- Adafruit SGP40
- WiFiManager, if required
- LiquidCrystal

Matter support comes from the ESP32 Arduino environment.

---

# Configuration constants

Important values currently defined in the firmware:

| Constant | Value | Purpose |
|---|---:|---|
| `pin_i2c_scl` | `0` | I²C clock |
| `pin_i2c_sda` | `1` | I²C data |
| `pin_motion` | `10` | PIR input |
| `pin_lcd_d7` | `2` | LCD D7 |
| `pin_lcd_d6` | `3` | LCD D6 |
| `pin_lcd_d5` | `4` | LCD D5 |
| `pin_lcd_d4` | `5` | LCD D4 |
| `pin_lcd_en` | `6` | LCD Enable |
| `pin_lcd_rs` | `7` | LCD Register Select |
| BME280 address | `0x76` | BME280 I²C address |
| Temperature threshold | `1.0 °C` | Matter reporting |
| Humidity threshold | `3.0 %` | Matter reporting |
| Pressure threshold | `1.0 hPa` | Matter reporting |
| Sensor interval | `1000 ms` | Sensor sampling |
| LCD interval | `5000 ms` | LCD refresh |
| Matter check | `15000 ms` | Connection monitoring |
| Matter timeout | `60000 ms` | Disconnection recovery |
