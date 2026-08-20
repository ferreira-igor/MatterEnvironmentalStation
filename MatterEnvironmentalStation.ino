/**
 * @file MatterEnvironmentalStation.ino
 * @brief Matter-compatible Environmental Station with LCD Display
 * 
 * This firmware implements a Matter-compatible environmental monitoring station
 * using an ESP32C3 with multiple sensors and a 20x4 LCD display. It provides:
 * - Matter protocol support for smart home integration
 * - Temperature, humidity, pressure, and VOC sensing
 * - Motion detection (HW-MS03 PIR sensor)
 * - 20x4 LCD display with custom icons (parallel interface)
 * - Visual status indicators for Matter commissioning
 * - Automatic commissioning method selection (BLE or WiFi)
 * - Automatic reconnection and error recovery
 * 
 * @author Igor Ferreira
 * @version 1.0
 * @date 2026
 * 
 * @hardware WeAct Studio ESP32C3 (tested)
 * @sensors:
 *   - BME280: Temperature, Humidity, Pressure (I2C address 0x76)
 *   - SGP40: VOC Index (I2C address 0x59)
 *   - HW-MS03: PIR Motion Sensor (Digital pin 10, 3.3V compatible)
 * @display: 20x4 LCD with parallel interface (HD44780 compatible)
 * @toolchain Arduino IDE 2.3.10
 * @core ESP32 Arduino Core 3.3.11
 * 
 * @note Commissioning Method Selection:
 *       - ESP32-C3: Uses BLE commissioning (CONFIG_ENABLE_CHIPOBLE enabled)
 *         WiFi credentials are provided by the Matter commissioner (Alexa,
 *         Google Home, Apple Home) during commissioning via BLE
 * 
 *       - ESP32 (without BLE) or when CONFIG_ENABLE_CHIPOBLE is disabled:
 *         Uses WiFi commissioning via WiFiManager captive portal
 *         User manually configures WiFi credentials through a web interface
 * 
 *       The code automatically adapts to the available commissioning method
 *       based on the CONFIG_ENABLE_CHIPOBLE compilation flag.
 * 
 * @note LED Behavior (Active LOW - typical for ESP32 boards):
 *       - LED ON (LOW): Device is NOT commissioned - ready for setup
 *       - LED OFF (HIGH): Device IS commissioned - operating normally
 * 
 * @note LCD Interface: Parallel 4-bit mode
 *       - RS (Register Select): pin 7
 *       - EN (Enable): pin 6
 *       - D4-D7 (Data lines): pins 5, 4, 3, 2
 *       - Uses LiquidCrystal library for HD44780-compatible displays
 * 
 * @note LCD Icons (custom characters stored in CGRAM):
 *       - Temperature (℃) - Index 0
 *       - Humidity (%) - Index 1
 *       - Pressure (hPa) - Index 2
 *       - Commissioned icon - Index 5
 *       - Decommissioned icon - Index 4
 *       - Connected icon - Index 7
 *       - Disconnected icon - Index 6
 * 
 * @note I2C pins: SCL=0, SDA=1 (WeAct Studio ESP32C3)
 *       LCD pins: RS=7, EN=6, D4=5, D5=4, D6=3, D7=2
 *       Motion sensor: HW-MS03 on pin 10 (3.3V logic level)
 */

#include <Arduino.h>

#include <Wire.h>
#include <Adafruit_BME280.h>  // https://github.com/adafruit/Adafruit_BME280_Library/releases/tag/2.3.0
#include <Adafruit_SGP40.h>   // https://github.com/adafruit/Adafruit_SGP40/releases/tag/1.1.4
#include <LiquidCrystal.h>

#include <Matter.h>

//=============================================================================
// Conditional WiFi Configuration
//=============================================================================

/**
 * @brief WiFiManager is included only when BLE commissioning is disabled
 * 
 * The commissioning method is determined at compile time:
 * 
 * 1. BLE Commissioning (CONFIG_ENABLE_CHIPOBLE = 1):
 *    - Used by ESP32-C3 and other boards with BLE support
 *    - WiFi credentials are provided by the Matter commissioner app
 *      (Alexa, Google Home, Apple Home) via BLE
 *    - WiFiManager is NOT used
 *    - This is the recommended method for most ESP32 boards
 * 
 * 2. WiFi Commissioning (CONFIG_ENABLE_CHIPOBLE = 0):
 *    - Used as fallback for boards without BLE or for debugging
 *    - ESP32 creates a captive portal WiFi network
 *    - User connects and configures WiFi via web interface
 *    - WiFiManager handles the configuration process
 * 
 * The conditional compilation ensures the code works on both BLE-capable
 * and non-BLE boards without modification.
 */
#if !CONFIG_ENABLE_CHIPOBLE
#include <WiFi.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager/releases/tag/v2.0.17

/** @brief WiFiManager instance for captive portal (BLE commissioning disabled) */
WiFiManager wm;
#endif

//=============================================================================
// Hardware Configuration Constants
//=============================================================================

//---------------------------------------------------------------------------
// I2C Pins (for BME280 and SGP40 sensors)
//---------------------------------------------------------------------------

/** @brief I2C SCL pin (GPIO 0 - WeAct Studio ESP32C3) */
constexpr uint8_t pin_i2c_scl = 0;

/** @brief I2C SDA pin (GPIO 1 - WeAct Studio ESP32C3) */
constexpr uint8_t pin_i2c_sda = 1;

//---------------------------------------------------------------------------
// LCD Parallel Interface Pins (4-bit mode)
//---------------------------------------------------------------------------

/**
 * @brief LCD D7 pin (GPIO 2) - Most significant data bit
 * 
 * In 4-bit mode, D7 is the highest-order data bit.
 * Data is sent in two 4-bit nibbles: high nibble (D4-D7) first,
 * followed by low nibble (D0-D3).
 */
constexpr uint8_t pin_lcd_d7 = 2;

/**
 * @brief LCD D6 pin (GPIO 3) - Data bit 6
 */
constexpr uint8_t pin_lcd_d6 = 3;

/**
 * @brief LCD D5 pin (GPIO 4) - Data bit 5
 */
constexpr uint8_t pin_lcd_d5 = 4;

/**
 * @brief LCD D4 pin (GPIO 5) - Data bit 4 (least significant in 4-bit mode)
 */
constexpr uint8_t pin_lcd_d4 = 5;

/**
 * @brief LCD Enable pin (GPIO 6)
 * 
 * A high-to-low pulse on this pin latches data from the data lines
 * into the LCD controller.
 */
constexpr uint8_t pin_lcd_en = 6;

/**
 * @brief LCD Register Select pin (GPIO 7)
 * 
 * Determines if data is interpreted as:
 * - LOW (0): Command/instruction
 * - HIGH (1): Character data to display
 */
constexpr uint8_t pin_lcd_rs = 7;

//---------------------------------------------------------------------------
// HW-MS03 PIR Motion Sensor
//---------------------------------------------------------------------------

/**
 * @brief HW-MS03 PIR Motion Sensor pin (GPIO 10)
 * 
 * HW-MS03 specifications:
 * - Operating Voltage: 3.3V to 5V DC (3.3V compatible)
 * - Output: Digital signal (3.3V logic level)
 * - Detection Range: Up to 7 meters (adjustable)
 * - Detection Angle: 120 degrees
 * - Output HIGH: Motion detected
 * - Output LOW: No motion detected
 * - Warm-up time: ~2 seconds after power-on
 * 
 * Connection:
 * - VCC: 3.3V
 * - GND: GND
 * - OUT: GPIO 10
 * 
 * Internal pulldown resistor ensures stable LOW when no motion.
 * 
 * @note HW-MS03 is a generic HC-SR501 variant with 3.3V compatibility
 * @note The sensor has adjustable sensitivity and time delay potentiometers
 */
constexpr uint8_t pin_motion = 10;

//---------------------------------------------------------------------------
// Built-in LED
//---------------------------------------------------------------------------

/**
 * @brief Built-in LED pin for status indication (Active LOW)
 * 
 * The LED is connected between VCC and the GPIO pin (common on ESP32 boards).
 * - LOW = LED ON (not commissioned - ready for setup)
 * - HIGH = LED OFF (commissioned - operating normally)
 * 
 * @note Active LOW is typical for ESP32 boards where the LED is connected
 *       to VCC through a current-limiting resistor
 */
constexpr uint8_t pin_led = LED_BUILTIN;

//=============================================================================
// Sensor Data Variables
//=============================================================================

/** @brief Current temperature reading in Celsius */
float current_temperature = 0.0f;

/** @brief Last temperature value reported to Matter */
float reported_temperature = 0.0f;

/**
 * @brief Temperature change threshold for reporting
 * 
 * Only report temperature changes greater than this value (1.0°C)
 * to avoid excessive Matter updates.
 * 
 * @note BME280 temperature accuracy is approximately ±1°C
 */
const float temperature_diff = 1.0f;

/** @brief Current humidity reading in percentage */
float current_humidity = 0.0f;

/** @brief Last humidity value reported to Matter */
float reported_humidity = 0.0f;

/**
 * @brief Humidity change threshold for reporting
 * 
 * Only report humidity changes greater than this value (3.0%)
 * to avoid excessive Matter updates.
 * 
 * @note BME280 humidity accuracy is approximately ±3%
 */
const float humidity_diff = 3.0f;

/** @brief Current pressure reading in hPa (hectopascals) */
float current_pressure = 0.0f;

/** @brief Last pressure value reported to Matter */
float reported_pressure = 0.0f;

/**
 * @brief Pressure change threshold for reporting
 * 
 * Only report pressure changes greater than this value (1.0 hPa)
 * to avoid excessive Matter updates.
 * 
 * @note BME280 pressure accuracy is approximately ±1 hPa
 */
const float pressure_diff = 1.0f;

/** @brief Current VOC (Volatile Organic Compounds) index from SGP40 */
int32_t current_voc = 0;

/** @brief Last VOC value reported to Matter (not currently used) */
int32_t reported_voc = 0;

/**
 * @brief Current motion detection state from HW-MS03 sensor
 * 
 * true = motion detected (sensor output HIGH)
 * false = no motion (sensor output LOW)
 */
bool current_motion = false;

/**
 * @brief Last motion state reported to Matter
 * 
 * Used to detect changes and avoid redundant updates.
 */
bool reported_motion = false;

//=============================================================================
// Timing Constants
//=============================================================================

/** @brief Interval between sensor readings */
const uint32_t sensors_read_interval = 1000;

/** @brief Interval for LCD display updates */
const uint32_t display_update_interval = 5000;

/** @brief Interval for Matter connection checks */
const uint32_t matter_check_interval = 15000;

/** @brief Maximum time without Matter connection before reboot */
const uint32_t matter_timeout = 180000;

/**
 * @brief Counter for consecutive Matter disconnections
 * 
 * Incremented when device is commissioned but not connected.
 * Reset to 0 when connection is restored.
 */
uint8_t matter_disconnect_counter = 0;

/** @brief Timestamp of last sensor reading */
uint32_t last_sensors_read = 0;

/** @brief Timestamp of last LCD display update */
uint32_t last_display_update = 0;

/** @brief Timestamp of last Matter connection check */
uint32_t last_matter_check = 0;

//=============================================================================
// LCD Configuration and Custom Icons
//=============================================================================

/** @brief Number of LCD columns (20 characters) */
constexpr uint8_t lcd_cols = 20;

/** @brief Number of LCD rows (4 lines) */
constexpr uint8_t lcd_rows = 4;

/**
 * @brief Custom LCD icon: Temperature symbol (℃)
 * 
 * 5x8 pixel bitmap representing a thermometer with degree symbol
 * Stored in LCD CGRAM (Character Generator RAM)
 */
uint8_t lcd_icon_temperature[8] = {
  0b00100,
  0b01010,
  0b01010,
  0b01010,
  0b01110,
  0b11111,
  0b11111,
  0b01110
};

/**
 * @brief Custom LCD icon: Humidity symbol (%)
 * 
 * 5x8 pixel bitmap representing a droplet for humidity
 * Stored in LCD CGRAM (Character Generator RAM)
 */
uint8_t lcd_icon_humidity[8] = {
  0b00100,
  0b00100,
  0b01010,
  0b01010,
  0b10001,
  0b10001,
  0b10001,
  0b01110
};

/**
 * @brief Custom LCD icon: Pressure symbol (hPa)
 * 
 * 5x8 pixel bitmap representing an aneroid barometer
 * Stored in LCD CGRAM (Character Generator RAM)
 */
uint8_t lcd_icon_pressure[8] = {
  0b11111,
  0b01110,
  0b00100,
  0b00000,
  0b00000,
  0b00100,
  0b01110,
  0b11111
};

/**
 * @brief Custom LCD icon: Decommissioned state
 * 
 * 5x8 pixel bitmap showing device is not commissioned
 * (Hourglass/sand timer indicating setup needed)
 * Stored in LCD CGRAM (Character Generator RAM)
 */
uint8_t lcd_icon_decommissioned[8] = {
  0b10100,
  0b01000,
  0b10100,
  0b00000,
  0b01010,
  0b10101,
  0b10101,
  0b10101
};

/**
 * @brief Custom LCD icon: Commissioned state
 * 
 * 5x8 pixel bitmap showing device is commissioned
 * (Check mark or confirmation symbol)
 * Stored in LCD CGRAM (Character Generator RAM)
 */
uint8_t lcd_icon_commissioned[8] = {
  0b00010,
  0b10100,
  0b01000,
  0b00000,
  0b01010,
  0b10101,
  0b10101,
  0b10101
};

/**
 * @brief Custom LCD icon: Disconnected state
 * 
 * 5x8 pixel bitmap showing Matter connection lost
 * (Broken chain or disconnected symbol)
 * Stored in LCD CGRAM (Character Generator RAM)
 */
uint8_t lcd_icon_disconnected[8] = {
  0b10100,
  0b01000,
  0b10100,
  0b00001,
  0b00011,
  0b00111,
  0b01111,
  0b11111
};

/**
 * @brief Custom LCD icon: Connected state
 * 
 * 5x8 pixel bitmap showing Matter connection active
 * (Chain link or connected symbol)
 * Stored in LCD CGRAM (Character Generator RAM)
 */
uint8_t lcd_icon_connected[8] = {
  0b00010,
  0b10100,
  0b01000,
  0b00001,
  0b00011,
  0b00111,
  0b01111,
  0b11111
};

//=============================================================================
// LCD Icon Index Definitions
//=============================================================================

/** @brief Index for temperature icon in LCD CGRAM (character 0) */
constexpr uint8_t lcd_index_temperature = 0;

/** @brief Index for humidity icon in LCD CGRAM (character 1) */
constexpr uint8_t lcd_index_humidity = 1;

/** @brief Index for pressure icon in LCD CGRAM (character 2) */
constexpr uint8_t lcd_index_pressure = 2;

/** @brief Index for decommissioned icon in LCD CGRAM (character 3) */
constexpr uint8_t lcd_index_decommissioned = 3;

/** @brief Index for commissioned icon in LCD CGRAM (character 4) */
constexpr uint8_t lcd_index_commissioned = 4;

/** @brief Index for disconnected icon in LCD CGRAM (character 5) */
constexpr uint8_t lcd_index_disconnected = 5;

/** @brief Index for connected icon in LCD CGRAM (character 6) */
constexpr uint8_t lcd_index_connected = 6;

//=============================================================================
// LCD and Sensor Initialization
//=============================================================================

/**
 * @brief LCD instance with parallel 4-bit interface
 * 
 * Connections (LiquidCrystal library pins):
 * - RS (Register Select): pin_lcd_rs (GPIO 7)
 * - EN (Enable): pin_lcd_en (GPIO 6)
 * - D4 (Data 4): pin_lcd_d4 (GPIO 5)
 * - D5 (Data 5): pin_lcd_d5 (GPIO 4)
 * - D6 (Data 6): pin_lcd_d6 (GPIO 3)
 * - D7 (Data 7): pin_lcd_d7 (GPIO 2)
 * 
 * The LiquidCrystal library handles the 4-bit communication protocol
 * automatically, sending data in two 4-bit nibbles (high nibble first).
 * 
 * @note HD44780-compatible LCD controller required
 * @note 4-bit mode uses only D4-D7 pins (D0-D3 are not connected)
 */
LiquidCrystal lcd(pin_lcd_rs, pin_lcd_en, pin_lcd_d4, pin_lcd_d5, pin_lcd_d6, pin_lcd_d7);

/** @brief I2C address for BME280 sensor (0x76 - can be 0x77 depending on ADDR pin) */
constexpr uint8_t bme280_addr = 0x76;

/** @brief BME280 sensor instance (temperature, humidity, pressure) */
Adafruit_BME280 bme;

/** @brief SGP40 sensor instance (VOC index - volatile organic compounds) */
Adafruit_SGP40 sgp;

//=============================================================================
// Matter Endpoints
//=============================================================================

/**
 * @brief Matter Temperature Sensor endpoint
 * 
 * Represents the temperature sensor as a Matter device.
 * Provides standard Matter temperature measurement cluster.
 */
MatterTemperatureSensor temperatureSensor;

/**
 * @brief Matter Humidity Sensor endpoint
 * 
 * Represents the humidity sensor as a Matter device.
 * Provides standard Matter humidity measurement cluster.
 */
MatterHumiditySensor humiditySensor;

/**
 * @brief Matter Pressure Sensor endpoint
 * 
 * Represents the pressure sensor as a Matter device.
 * Provides standard Matter pressure measurement cluster.
 */
MatterPressureSensor pressureSensor;

/**
 * @brief Matter Occupancy Sensor endpoint
 * 
 * Represents the motion sensor (HW-MS03) as a Matter device.
 * Provides standard Matter occupancy measurement cluster.
 */
MatterOccupancySensor occupancySensor;

//=============================================================================
// Sensor Reading Functions
//=============================================================================

/**
 * @brief Read all sensors and update current values
 * 
 * Reads data from BME280 (temperature, humidity, pressure),
 * SGP40 (VOC index), and HW-MS03 PIR motion sensor.
 * 
 * Sensor details:
 * - BME280 (I2C address 0x76):
 *   * Temperature: ±1°C accuracy, -40°C to +85°C range
 *   * Humidity: ±3% accuracy, 0-100% range
 *   * Pressure: ±1 hPa accuracy, 300-1100 hPa range
 * 
 * - SGP40 (I2C address 0x59):
 *   * VOC Index: 0-500 scale (relative air quality)
 *   * Requires temperature and humidity for compensation
 *   * Higher values indicate more VOCs in the air
 *   * Uses MOX (metal-oxide) technology
 * 
 * - HW-MS03 (PIR Motion Sensor on pin 10):
 *   * Output HIGH (3.3V) when motion detected
 *   * Output LOW (0V) when no motion
 *   * Detection range: up to 7 meters
 *   * Detection angle: 120 degrees
 *   * Warm-up time: ~2 seconds
 * 
 * @note Called every sensors_read_interval
 * @note SGP40 reading uses current temperature and humidity for compensation
 * @note All readings are validated (checked for NaN)
 * 
 * @warning If a sensor fails, an error message is printed but reading continues
 */
void readSensors() {
  // Read temperature from BME280
  float t = bme.readTemperature();

  if (!isnan(t)) {
    current_temperature = t;
    Serial.print("Temperature: ");
    Serial.print(current_temperature);
    Serial.println("C");
  } else {
    Serial.println("Error reading temperature!");
  }

  // Read humidity from BME280
  float h = bme.readHumidity();

  if (!isnan(h)) {
    current_humidity = h;
    Serial.print("Humidity: ");
    Serial.print(current_humidity);
    Serial.println("%");
  } else {
    Serial.println("Error reading humidity!");
  }

  // Read pressure from BME280 (convert Pa to hPa)
  float p = bme.readPressure();

  if (!isnan(p)) {
    current_pressure = p / 100.0f;  // Convert Pa to hPa
    Serial.print("Pressure: ");
    Serial.print(current_pressure);
    Serial.println("hPA");
  } else {
    Serial.println("Error reading pressure!");
  }

  // Read VOC index from SGP40 (compensated with temperature and humidity)
  current_voc = sgp.measureVocIndex(current_temperature, current_humidity);
  Serial.print("VOC Index: ");
  Serial.println(current_voc);

  // Read HW-MS03 PIR motion sensor state
  // HIGH = motion detected, LOW = no motion
  current_motion = digitalRead(pin_motion);
  Serial.print("Motion: ");
  Serial.println(current_motion);
}

//=============================================================================
// LCD Display Functions
//=============================================================================

/**
 * @brief Update LCD display with current sensor readings and status
 * 
 * Displays on a 20x4 LCD (HD44780-compatible):
 * 
 * Row 0 (Status Line - 20 chars):
 * ```
 * [icon] Pin: 12345678 [icon]     <- Commissioning mode
 * [icon]    Room Stats    [icon]  <- Normal mode
 *  ^       ^                ^
 *  |       |                └─ Connection status (connected/disconnected)
 *  |       └─ Center text (pairing code or "Room Stats")
 *  └─ Commissioning status (decommissioned/commissioned)
 * ```
 * 
 * Row 1 (Motion Status - 20 chars):
 * ```
 *   Motion Detected!      <- Motion detected (HW-MS03 triggered)
 * ====================   <- No motion
 * ```
 * 
 * Row 2 (Sensor Values - 20 chars):
 * ```
 * [℃] 25C  [%] 65%  [hPa] 1013hPa
 *  ^     ^   ^     ^    ^      ^
 *  |     |   |     |    |      └─ Pressure in hPa
 *  |     |   |     |    └─ Icon: pressure (hPa)
 *  |     |   |     └─ Humidity percentage
 *  |     |   └─ Icon: humidity (%)
 *  |     └─ Temperature in Celsius (rounded)
 *  └─ Icon: temperature (℃)
 * ```
 * 
 * Row 3 (Air Quality - 20 chars):
 * ```
 *   Air is Excellent    <- VOC index ≤ 100
 *     Air is Good       <- VOC index ≤ 150
 *   Air is Moderate     <- VOC index ≤ 250
 *     Air is Poor       <- VOC index ≤ 400
 *   Air is Unhealthy    <- VOC index > 400
 * ```
 * 
 * VOC Index Scale (SGP40):
 * - 0-100: Excellent air quality
 * - 101-150: Good air quality
 * - 151-250: Moderate air quality
 * - 251-400: Poor air quality
 * - > 400: Unhealthy air quality
 * 
 * @note Called every display_update_interval
 * @note Uses lround() for rounding sensor values to integers
 * @note LCD memory positions: (col, row) starting from (0,0)
 * @note Custom characters are stored in LCD CGRAM (indices 0-7)
 */
void updateDisplay() {

  // Row 0: Matter Status + Pairing Code + Connection Status
  lcd.setCursor(0, 0);

  // Commissioning status icon
  if (!Matter.isDeviceCommissioned()) {
    lcd.write(lcd_index_decommissioned);
    lcd.print(" Pin: ");
    lcd.print(Matter.getManualPairingCode().c_str());
    lcd.print(" ");
  } else {
    lcd.write(lcd_index_commissioned);
    lcd.print("    Room Stats    ");
  }

  // Connection status icon
  if (!Matter.isDeviceConnected()) {
    lcd.write(lcd_index_disconnected);
  } else {
    lcd.write(lcd_index_connected);
  }

  // Row 1: Motion Detection Status (from HW-MS03 sensor)
  lcd.setCursor(0, 1);
  if (current_motion) {
    lcd.print("  Motion Detected!  ");
  } else {
    lcd.print("====================");
  }

  // Row 2: Sensor Readings
  lcd.setCursor(0, 2);
  lcd.write(lcd_index_temperature);
  lcd.print(lround(current_temperature));
  lcd.print("C  ");
  lcd.write(lcd_index_humidity);
  lcd.print(lround(current_humidity));
  lcd.print("%  ");
  lcd.write(lcd_index_pressure);
  lcd.print(lround(current_pressure));
  lcd.print("hPa");

  // Row 3: Air Quality Description (based on VOC index)
  lcd.setCursor(0, 3);
  if (current_voc <= 100) {
    lcd.print("  Air is Excellent  ");
  } else if (current_voc <= 150) {
    lcd.print("    Air is Good     ");
  } else if (current_voc <= 250) {
    lcd.print("  Air is Moderate   ");
  } else if (current_voc <= 400) {
    lcd.print("    Air is Poor     ");
  } else {
    lcd.print("  Air is Unhealthy  ");
  }
}

//=============================================================================
// Matter Update Functions
//=============================================================================

/**
 * @brief Update Matter endpoints with new sensor data
 * 
 * Checks if the device is commissioned and connected to a Matter hub.
 * Compares current readings with last reported values and updates
 * Matter endpoints only when changes exceed configured thresholds.
 * 
 * Endpoints updated:
 * - Temperature: Update when change ≥ 1.0°C (BME280 accuracy)
 * - Humidity: Update when change ≥ 3.0% (BME280 accuracy)
 * - Pressure: Update when change ≥ 1.0 hPa (BME280 accuracy)
 * - Occupancy: Update on any state change (immediate for motion detection)
 * 
 * Thresholds are based on sensor accuracy:
 * - BME280 temperature: ±1°C
 * - BME280 humidity: ±3%
 * - BME280 pressure: ±1 hPa
 * - HW-MS03 motion: Binary (on/off) - always report changes
 * 
 * This prevents unnecessary network traffic and Matter updates
 * when sensor values fluctuate minimally.
 * 
 * @note Only updates if device is commissioned (Matter.isDeviceCommissioned())
 * @note Motion updates are sent immediately on any state change
 * @note VOC is not reported to Matter (display only - no standard Matter cluster)
 */
void updateMatter() {

  // Only update if device is commissioned
  if (Matter.isDeviceCommissioned()) {

    // Update temperature if change exceeds threshold
    if (fabsf(current_temperature - reported_temperature) >= temperature_diff) {
      temperatureSensor.setTemperature(current_temperature);
      reported_temperature = current_temperature;
    }

    // Update humidity if change exceeds threshold
    if (fabsf(current_humidity - reported_humidity) >= humidity_diff) {
      humiditySensor.setHumidity(current_humidity);
      reported_humidity = current_humidity;
    }

    // Update pressure if change exceeds threshold
    if (fabsf(current_pressure - reported_pressure) >= pressure_diff) {
      pressureSensor.setPressure(current_pressure);
      reported_pressure = current_pressure;
    }

    // Update motion on any state change (immediate reporting)
    if (current_motion != reported_motion) {
      occupancySensor.setOccupancy(current_motion);
      reported_motion = current_motion;
    }
  }
}

//=============================================================================
// Matter Connection Management
//=============================================================================

/**
 * @brief Check Matter connection status and handle disconnections
 * 
 * Monitors the Matter connection state and implements a recovery strategy:
 * 1. Updates LED status based on commissioning state (Active LOW)
 * 2. Tracks disconnection duration
 * 3. Automatically decommissions and reboots after 60 seconds of disconnection
 * 
 * LED Behavior (Active LOW - LED ON when pin is LOW):
 * - LED ON (LOW):  Device is NOT commissioned - ready for commissioning
 * - LED OFF (HIGH): Device IS commissioned - operating normally
 * 
 * This provides clear visual feedback:
 * - ON LED indicates device needs to be set up
 * - OFF LED indicates device is already in your smart home
 * 
 * Recovery Strategy:
 * - Counts consecutive check cycles where device is commissioned but disconnected
 * - If disconnection persists for matter_timeout, decommission and reboot
 * - This forces the device to be re-added to the Matter network
 * 
 * @note Check interval is defined by matter_check_interval
 * @note matter_disconnect_counter increments each check cycle when disconnected
 * @note Counter resets to 0 when connection is restored
 */
void checkMatter() {

  // Update LED based on commission status (Active LOW)
  // LED ON (LOW)  = Not commissioned - ready for setup
  // LED OFF (HIGH) = Commissioned - operating normally
  if (!Matter.isDeviceCommissioned()) {
    digitalWrite(pin_led, LOW);  // LED ON - waiting for commissioning
  } else {
    digitalWrite(pin_led, HIGH);  // LED OFF - commissioned and running
  }

  // Detect disconnection scenario:
  // Device is commissioned (known to hub) but not connected (communication lost)
  if (Matter.isDeviceCommissioned() != Matter.isDeviceConnected()) {
    matter_disconnect_counter++;
    Serial.println("Device disconnected!");

    // If disconnected for longer than matter_timeout
    if (matter_disconnect_counter >= (matter_timeout / matter_check_interval)) {
      Serial.println("The connection was lost! Decommissioning and rebooting...");

      // Decommission the device (removes it from the Matter network)
      Matter.decommission();
      matter_disconnect_counter = 0;

      // Allow time for decommissioning to complete
      delay(1000);

      // Reboot to start fresh
      ESP.restart();
    }
  } else {
    // Connection restored or still connected - reset counter
    matter_disconnect_counter = 0;
  }
}

//=============================================================================
// Setup Function
//=============================================================================

/**
 * @brief Arduino setup function
 * 
 * Initializes the system in the following order:
 * 1. Hardware initialization (pins, LED, serial, I2C)
 * 2. LCD initialization and custom character creation
 * 3. Sensor initialization (BME280, SGP40)
 * 4. WiFi configuration (method depends on CONFIG_ENABLE_CHIPOBLE)
 * 5. Matter endpoint initialization (temperature, humidity, pressure, occupancy)
 * 6. Matter protocol initialization
 * 7. Display commissioning information (pairing code, QR code)
 * 
 * WiFi Configuration Methods:
 * 
 * A) BLE Commissioning (CONFIG_ENABLE_CHIPOBLE = 1):
 *    - Used by ESP32-C3 and BLE-capable boards
 *    - WiFi credentials are NOT defined in code
 *    - The Matter commissioner (Alexa, Google Home, Apple Home) app
 *      provides WiFi credentials during commissioning via BLE
 *    - No user interaction required for WiFi setup
 *    - This is the standard Matter commissioning flow
 * 
 * B) WiFi Commissioning (CONFIG_ENABLE_CHIPOBLE = 0):
 *    - Used by boards without BLE or for debugging
 *    - ESP32 creates a captive portal WiFi network
 *    - User connects to the ESP32's AP and configures WiFi via web
 *    - WiFiManager handles the configuration process
 *    - Useful for testing or when BLE is not available
 * 
 * LCD Custom Characters (stored in CGRAM):
 * - Temperature symbol (℃) - Index 0
 * - Humidity symbol (%) - Index 1
 * - Pressure symbol (hPa) - Index 2
 * - Commissioned icon - Index 4
 * - Decommissioned icon - Index 3
 * - Connected icon - Index 6
 * - Disconnected icon - Index 5
 * 
 * LED Behavior (Active LOW):
 * - LED ON (LOW) during initialization
 * - After setup, LED state is managed by checkMatter():
 *   * LED ON (LOW)  = Not commissioned - ready for setup
 *   * LED OFF (HIGH) = Commissioned - operating normally
 * 
 * @note I2C pins: SDA=1, SCL=0 (WeAct Studio ESP32C3)
 * @note LCD pins (parallel 4-bit mode): RS=7, EN=6, D4=5, D5=4, D6=3, D7=2
 * @note HW-MS03 motion sensor: pin 10 (INPUT_PULLDOWN)
 * 
 * @warning If BME280 initialization fails, error message is printed but setup continues
 * @warning If SGP40 initialization fails, error message is printed but setup continues
 * @warning If WiFi commissioning fails (WiFiManager mode), the device reboots
 */
void setup() {

  //-----------------------------------------------------------------------
  // 1. Hardware Initialization
  //-----------------------------------------------------------------------

  // HW-MS03 PIR motion sensor: digital input with internal pulldown
  // Ensures stable LOW when no motion is detected
  pinMode(pin_motion, INPUT_PULLDOWN);

  // Built-in LED: output (Active LOW)
  pinMode(pin_led, OUTPUT);

  // LED ON during initialization (Active LOW)
  digitalWrite(pin_led, HIGH);

  Serial.begin(115200);

  // Allow time for serial to initialize
  delay(1000);

  //-----------------------------------------------------------------------
  // 2. LCD Initialization and Custom Characters
  //-----------------------------------------------------------------------

  // Initialize 20x4 LCD with parallel 4-bit interface
  lcd.begin(lcd_cols, lcd_rows);

  // Create custom characters in LCD CGRAM (Character Generator RAM)
  // These can be displayed using lcd.write(index)
  lcd.createChar(lcd_index_temperature, lcd_icon_temperature);
  lcd.createChar(lcd_index_humidity, lcd_icon_humidity);
  lcd.createChar(lcd_index_pressure, lcd_icon_pressure);
  lcd.createChar(lcd_index_commissioned, lcd_icon_commissioned);
  lcd.createChar(lcd_index_decommissioned, lcd_icon_decommissioned);
  lcd.createChar(lcd_index_connected, lcd_icon_connected);
  lcd.createChar(lcd_index_disconnected, lcd_icon_disconnected);

  //-----------------------------------------------------------------------
  // 3. Sensor Initialization
  //-----------------------------------------------------------------------

  // Initialize I2C bus for sensors
  Wire.begin(pin_i2c_sda, pin_i2c_scl);

  // Initialize BME280 (temperature, humidity, pressure)
  while (!bme.begin(bme280_addr)) {
    Serial.println("Error initializing BME280 sensor! Check your wiring!");
    delay(1000);
  }

  // Initialize SGP40 (VOC index sensor)
  while (!sgp.begin()) {
    Serial.println("Error initializing SGP40 sensor! Check your wiring!");
    delay(1000);
  }

  //-----------------------------------------------------------------------
  // 4. WiFi Configuration (Conditional based on commissioning method)
  //-----------------------------------------------------------------------

  /**
   * WiFi configuration method is selected at compile time:
   * 
   * - If CONFIG_ENABLE_CHIPOBLE is defined (default for ESP32-C3):
   *   BLE commissioning is used. WiFi credentials are provided by the
   *   Matter commissioner app during commissioning. No manual setup needed.
   * 
   * - If CONFIG_ENABLE_CHIPOBLE is NOT defined (fallback):
   *   WiFiManager captive portal is used. User manually configures WiFi
   *   credentials through a web interface. Useful for boards without BLE
   *   or for debugging purposes.
   * 
   * This design ensures compatibility with both BLE-capable and non-BLE
   * ESP32 boards without code changes.
   */
#if !CONFIG_ENABLE_CHIPOBLE
  // WiFi Commissioning Mode (Captive Portal)
  std::vector<const char *> menu = { "wifi", "restart", "exit" };
  wm.setMenu(menu);
  wm.setConfigPortalTimeout(180);  // 3 minutes timeout
  wm.setConnectTimeout(30);        // 30 seconds connection timeout

  if (!wm.autoConnect()) {
    Serial.println("Could not connect to WiFi! Rebooting...");
    delay(1000);
    ESP.restart();
  }
#endif

  //-----------------------------------------------------------------------
  // 5. Matter Endpoint Initialization
  //-----------------------------------------------------------------------

  if (!temperatureSensor.begin()) {
    Serial.println("Error initializing temperature endpoint!");
  }

  if (!humiditySensor.begin()) {
    Serial.println("Error initializing humidity endpoint!");
  }

  if (!pressureSensor.begin()) {
    Serial.println("Error initializing pressure endpoint!");
  }

  if (!occupancySensor.begin()) {
    Serial.println("Error initializing occupancy endpoint!");
  }

  //-----------------------------------------------------------------------
  // 6. Matter Protocol Initialization
  //-----------------------------------------------------------------------

  Matter.begin();

  //-----------------------------------------------------------------------
  // 7. Commissioning Information
  //-----------------------------------------------------------------------

  if (!Matter.isDeviceCommissioned()) {
    Serial.println("Matter Node is not commissioned yet.");
    Serial.println("Initiate the device discovery in your Matter environment.");
    Serial.println("Commission it to your Matter hub with the manual pairing code or QR code");
    Serial.print("Manual pairing code: ");
    Serial.println(Matter.getManualPairingCode().c_str());
    Serial.print("QR code URL: ");
    Serial.println(Matter.getOnboardingQRCodeUrl().c_str());
  }
}

//=============================================================================
// Loop Function
//=============================================================================

/**
 * @brief Arduino main loop
 * 
 * Manages three periodic tasks using non-blocking timers:
 * 
 * 1. Sensor Reading:
 *    - Reads temperature, humidity, pressure from BME280
 *    - Reads VOC index from SGP40
 *    - Reads motion state from HW-MS03 PIR sensor
 *    - Updates Matter endpoints if changes exceed thresholds
 * 
 * 2. LCD Display Update:
 *    - Updates all LCD display lines (20x4 characters)
 *    - Shows Matter status, sensor values, and air quality
 *    - Reduces display flicker by not updating every second
 *    - Custom icons are displayed using lcd.write(index)
 * 
 * 3. Matter Connection Check:
 *    - Monitors commission and connection status
 *    - Handles disconnection recovery
 *    - Updates LED status (Active LOW)
 * 
 * @note Uses millis() for timing instead of delay() to keep loop non-blocking
 * @note All tasks run independently at their configured intervals
 * @note The display update interval is longer to reduce LCD flicker
 * @note HW-MS03 motion sensor updates are immediate (real-time detection)
 */
void loop() {
  uint32_t current_time = millis();

  // Task 1: Read sensors
  if (current_time - last_sensors_read >= sensors_read_interval) {
    last_sensors_read = current_time;
    readSensors();
    updateMatter();
  }

  // Task 2: Update LCD display
  if (current_time - last_display_update >= display_update_interval) {
    last_display_update = current_time;
    updateDisplay();
  }

  // Task 3: Check Matter connection
  if (current_time - last_matter_check >= matter_check_interval) {
    last_matter_check = current_time;
    checkMatter();
  }
}