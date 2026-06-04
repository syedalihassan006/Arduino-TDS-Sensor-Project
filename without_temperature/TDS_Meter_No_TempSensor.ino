/*
 * ============================================================
 *  Arduino TDS Salinity Sensor
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
 *  Reads TDS (Total Dissolved Solids) in ppm from a Keystudio
 *  conductivity probe. Displays TDS, EC, and temperature on a
 *  16x2 LCD. Triggers a red LED and buzzer alert if readings
 *  fall outside the safe range (50–700 ppm).
 *
 *  HARDWARE:
 *  - Arduino Uno
 *  - Keystudio TDS/EC probe (analog)
 *  - 16x2 LCD Display
 *  - Green LED (safe indicator)
 *  - Red LED + Buzzer (out-of-range alert)
 *  - 9V Battery
 *
 *  PIN CONNECTIONS:
 *  - A0  → TDS Sensor Signal
 *  - 2–7 → LCD (RS, EN, D4, D5, D6, D7)
 *  - 8   → Green LED
 *  - 9   → Red LED
 *  - 13  → Buzzer
 *
 *  LIBRARY REQUIRED:
 *  - LiquidCrystal.h (built into Arduino IDE)
 * ============================================================
 */

#include <LiquidCrystal.h>

// --- LCD Pin Configuration ---
LiquidCrystal lcd(2, 3, 4, 5, 6, 7);  // RS, EN, D4, D5, D6, D7

// --- Pin Definitions ---
#define tds_sensor A0   // TDS probe analog input
#define G_led      8    // Green LED (safe range)
#define R_led      9    // Red LED (out of range)
#define buzzer     13   // Buzzer (out of range alert)

// --- Sensor Calibration Settings ---
float aref          = 4.3;  // Arduino analog reference voltage (V)
float ecCalibration = 1.0;  // EC calibration factor (adjust during calibration)

// --- Sensor Variables ---
float ec            = 0;     // Electrical conductivity value
unsigned int tds    = 0;     // TDS value in ppm

// --- Temperature Setting ---
// Fixed at 25°C (no temp sensor in this version)
// Future: integrate DS18B20 for real-time compensation
float waterTemp = 25.0;

// ============================================================
void setup() {
  // Set pin modes
  pinMode(tds_sensor, INPUT);
  pinMode(R_led, OUTPUT);
  pinMode(G_led, OUTPUT);
  pinMode(buzzer, OUTPUT);

  // Initialize LCD and show welcome message
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   Welcome To   ");
  lcd.setCursor(0, 1);
  lcd.print("TDS Meter Only");
  delay(2000);
  lcd.clear();
}

// ============================================================
void loop() {

  // --- Step 1: Read raw analog voltage from TDS sensor ---
  float rawEc = analogRead(tds_sensor) * aref / 1024.0;

  // --- Step 2: Apply temperature compensation ---
  // Formula: compensated EC = raw EC / (1 + 0.02 * (T - 25))
  float temperatureCoefficient = 1.0 + 0.02 * (waterTemp - 25.0);
  ec = (rawEc / temperatureCoefficient) * ecCalibration;

  // --- Step 3: Convert EC to TDS (ppm) ---
  // Manufacturer-provided polynomial formula
  tds = (133.42 * pow(ec, 3) - 255.86 * ec * ec + 857.39 * ec) * 0.5;

  // --- Step 4: Display readings on LCD ---
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TDS   EC   Temp");  // Header row
  lcd.setCursor(0, 1);
  lcd.print(tds);                // TDS in ppm
  lcd.setCursor(5, 1);
  lcd.print(ec, 2);              // EC with 2 decimal places
  lcd.setCursor(11, 1);
  lcd.print(waterTemp, 1);       // Temperature (fixed 25.0°C)

  // --- Step 5: Check safe range and trigger alerts ---
  // Safe range: 50–700 ppm (potable/usable water)
  if (tds < 50 || tds > 700) {
    digitalWrite(buzzer, HIGH);  // Sound buzzer
    digitalWrite(G_led, LOW);   // Turn off green LED
    digitalWrite(R_led, HIGH);  // Turn on red LED
    delay(300);
  } else {
    digitalWrite(G_led, HIGH);  // Turn on green LED (safe)
    digitalWrite(R_led, LOW);   // Turn off red LED
  }

  digitalWrite(buzzer, LOW);    // Ensure buzzer is off after delay
  delay(500);                   // Wait 500ms before next reading
}
