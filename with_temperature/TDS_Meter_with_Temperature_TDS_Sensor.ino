/*
 * ============================================================
 *  Arduino TDS Salinity Sensor — With Temperature Compensation
 * ============================================================
 *  Project:    Water Quality Assessment Using Arduino-Driven
 *              Salinity Sensor
 *  Course:     ME 5840 - Advanced Experimental Methods
 *  School:     California State University, Los Angeles
 *  Authors:    Syed Ali Hassan, Sagar Timbadiya,
 *              Chirag Patel, Sujeeth P.K
 *  Date:       May 14, 2025
 * ============================================================
 *
 *  DESCRIPTION:
 *  Improved version of the TDS sensor with real-time temperature
 *  compensation using a DS18B20 temperature sensor. Reads TDS
 *  (Total Dissolved Solids) in ppm from a Keystudio conductivity
 *  probe, compensates for water temperature, and displays TDS,
 *  EC, and live temperature on a 16x2 LCD.
 *  Triggers a red LED and buzzer alert if readings fall outside
 *  the safe range (50–700 ppm).
 *
 *  HARDWARE:
 *  - Arduino Uno
 *  - Keystudio TDS/EC probe (analog)
 *  - DS18B20 Temperature Sensor (1-Wire, no library needed)
 *  - 16x2 LCD Display
 *  - Green LED (safe indicator)
 *  - Red LED + Buzzer (out-of-range alert)
 *  - 9V Battery
 *
 *  PIN CONNECTIONS:
 *  - A0  → TDS Sensor Signal
 *  - A5  → DS18B20 Temperature Sensor (1-Wire)
 *  - 2–7 → LCD (RS, EN, D4, D5, D6, D7)
 *  - 8   → Green LED
 *  - 9   → Red LED
 *  - 13  → Buzzer
 *
 *  LIBRARY REQUIRED:
 *  - LiquidCrystal.h (built into Arduino IDE)
 *  Note: DS18B20 is driven manually via 1-Wire protocol
 *        (no external library needed)
 * ============================================================
 */

#include <LiquidCrystal.h>

// --- LCD Pin Configuration ---
LiquidCrystal lcd(2, 3, 4, 5, 6, 7);  // RS, EN, D4, D5, D6, D7

// --- Pin Definitions ---
#define tds_sensor   A0   // TDS probe analog input
#define DS18B20_PIN  A5   // DS18B20 temperature sensor (1-Wire)
#define G_led        8    // Green LED (safe range)
#define R_led        9    // Red LED (out of range)
#define buzzer       13   // Buzzer (out of range alert)

// --- Sensor Calibration Settings ---
float aref          = 4.3;  // Arduino analog reference voltage (V)
float ecCalibration = 1.0;  // EC calibration factor (adjust during calibration)

// --- Sensor Variables ---
float ec         = 0;    // Electrical conductivity value
unsigned int tds = 0;    // TDS value in ppm

// --- Temperature Variables ---
int   raw_temp   = 0;    // Raw temperature reading from DS18B20
float waterTemp  = 0;    // Converted temperature in °C

// ============================================================
//  DS18B20 1-Wire Protocol Functions (no library needed)
// ============================================================

// Send reset pulse and check if DS18B20 is present
bool ds18b20_start() {
  bool ret = 0;
  digitalWrite(DS18B20_PIN, LOW);       // Pull line LOW to reset
  pinMode(DS18B20_PIN, OUTPUT);
  delayMicroseconds(500);               // Hold LOW for 500µs
  pinMode(DS18B20_PIN, INPUT);          // Release line
  delayMicroseconds(100);               // Wait for sensor response
  if (!digitalRead(DS18B20_PIN)) {
    ret = 1;                            // Sensor pulled line LOW = present
    delayMicroseconds(400);             // Wait for presence pulse to finish
  }
  return ret;
}

// Write a single bit to the DS18B20
void ds18b20_write_bit(bool value) {
  digitalWrite(DS18B20_PIN, LOW);
  pinMode(DS18B20_PIN, OUTPUT);
  delayMicroseconds(2);
  digitalWrite(DS18B20_PIN, value);
  delayMicroseconds(80);
  pinMode(DS18B20_PIN, INPUT);
  delayMicroseconds(2);
}

// Write a full byte to the DS18B20 (LSB first)
void ds18b20_write_byte(byte value) {
  for (byte i = 0; i < 8; i++)
    ds18b20_write_bit(bitRead(value, i));
}

// Read a single bit from the DS18B20
bool ds18b20_read_bit() {
  bool value;
  digitalWrite(DS18B20_PIN, LOW);
  pinMode(DS18B20_PIN, OUTPUT);
  delayMicroseconds(2);
  pinMode(DS18B20_PIN, INPUT);          // Release line for sensor to respond
  delayMicroseconds(5);
  value = digitalRead(DS18B20_PIN);
  delayMicroseconds(100);
  return value;
}

// Read a full byte from the DS18B20 (LSB first)
byte ds18b20_read_byte() {
  byte value = 0;
  for (byte i = 0; i < 8; i++)
    bitWrite(value, i, ds18b20_read_bit());
  return value;
}

// Full temperature read sequence:
// Start conversion → wait → read scratchpad → return raw temp
bool ds18b20_read(int *raw_temp_value) {
  if (!ds18b20_start()) return 0;       // Reset; return 0 if no sensor found
  ds18b20_write_byte(0xCC);             // Skip ROM (only one sensor on bus)
  ds18b20_write_byte(0x44);             // Start temperature conversion
  while (ds18b20_read_byte() == 0);     // Wait until conversion is complete

  if (!ds18b20_start()) return 0;       // Reset again to read result
  ds18b20_write_byte(0xCC);             // Skip ROM
  ds18b20_write_byte(0xBE);             // Read scratchpad (temperature data)

  // Read 2 bytes (LSB and MSB) and combine into 16-bit raw value
  *raw_temp_value  = ds18b20_read_byte();
  *raw_temp_value |= (unsigned int)(ds18b20_read_byte() << 8);
  return 1;                             // Success
}

// ============================================================
void setup() {
  pinMode(tds_sensor, INPUT);
  pinMode(R_led, OUTPUT);
  pinMode(G_led, OUTPUT);
  pinMode(buzzer, OUTPUT);

  // Show welcome message on LCD
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   Welcome To   ");
  lcd.setCursor(0, 1);
  lcd.print("TDS & Temp Meter");
  delay(2000);
  lcd.clear();
}

// ============================================================
void loop() {

  // --- Step 1: Read live temperature from DS18B20 ---
  if (ds18b20_read(&raw_temp)) {
    waterTemp = (float)raw_temp / 16.0;  // Convert raw to °C (raw / 16)
  }

  // --- Step 2: Read raw analog voltage from TDS sensor ---
  float rawEc = analogRead(tds_sensor) * aref / 1024.0;

  // --- Step 3: Apply real-time temperature compensation ---
  // Formula: compensated EC = raw EC / (1 + 0.02 * (T - 25))
  float temperatureCoefficient = 1.0 + 0.02 * (waterTemp - 25.0);
  ec = (rawEc / temperatureCoefficient) * ecCalibration;

  // --- Step 4: Convert EC to TDS (ppm) ---
  tds = (133.42 * pow(ec, 3) - 255.86 * ec * ec + 857.39 * ec) * 0.5;

  // --- Step 5: Display readings on LCD ---
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TDS   EC   Temp");   // Header row
  lcd.setCursor(0, 1);
  lcd.print(tds);                 // TDS in ppm
  lcd.setCursor(5, 1);
  lcd.print(ec, 2);               // EC with 2 decimal places
  lcd.setCursor(11, 1);
  lcd.print(waterTemp, 2);        // Live temperature in °C

  // --- Step 6: Check safe range and trigger alerts ---
  if (tds < 50 || tds > 700) {
    digitalWrite(buzzer, HIGH);   // Sound buzzer
    digitalWrite(G_led, LOW);    // Turn off green LED
    digitalWrite(R_led, HIGH);   // Turn on red LED
    delay(300);
  } else {
    digitalWrite(G_led, HIGH);   // Turn on green LED (safe)
    digitalWrite(R_led, LOW);    // Turn off red LED
  }

  digitalWrite(buzzer, LOW);     // Ensure buzzer is off after delay
  delay(500);                    // Wait 500ms before next reading
}
