/*
(1) 74HC595:
    Control pins -> MCU:
        SRCLK -> IO3
        RCLK -> IO10
        SER -> IO9
        QH' -> (2) 74HC595 SER
    Outputs -> Displays:
        QA -> CLR
        QB -> A0
        QC -> A1
        QD -> Display 1 WR
        QE -> Display 2 WR
        QF -> Display 3 WR
        QG -> Display 4 WR
        QH -> Display 5 WR

(2) 74HC595:
    Control pins -> MCU:
        SRCLK -> IO3
        RCLK -> IO10
        SER -> (1) 74HC595 QH'
    Outputs -> Displays
        QA -> D0
        QB -> D1
        QC -> D2
        QD -> D3
        QE -> D4
        QF -> D5
        QG -> D6

GPS -> MCU:
    V -> 3.3V
    G -> GND
    T -> IO4
    R -> NC

DHT11 -> MCU:
    VCC -> 5V
    GND -> GND
    DATA -> IO5

DS3231M -> MCU:
    VCC -> 5V
    GND -> GND
    VBAT -> CR2032 +
    SDA -> IO6 (pullup with 4.7k -> 3.3V)
    SCL -> IO7 (pullup with 4.7k -> 3.3V)

Buzzer -> MCU:
    + -> IO8
    - -> GND

JoyStick -> MCU:
  X -> IO0
  SW -> IO1
  Y -> IO2
*/

#include "DL-display.h"
#include <Wire.h>
#include "RTClib.h"
#include <DHT.h>
#include "Better-GPS.h"
#include "Better-JoyStick.h"

// Pins
const byte SRCLK = 3;
const byte RCLK = 10;
const byte SER = 9;
const byte JS_SW = 1;
const byte JS_X = 0;
const byte JS_Y = 2;
const byte DHT_SENSOR = 5;
const byte BUZZER = 8;
const byte GPS_RX = 4;
const byte RTC_SDA = 6;
const byte RTC_SCL = 7;

// Display
DLDisplay display;

// DHT sensor
#define DHT_TYPE DHT11
DHT dht(DHT_SENSOR, DHT_TYPE);

// GPS
BetterGPS gps;

// RTC
RTC_DS3231 rtc;

// JoyStick
BetterJoystick joystick;

// DHT variables
float temperature = 0.0;
float humidity = 0.0;

// Sync interval
const unsigned long RTC_SYNC_INTERVAL = 10UL * 60UL * 1000UL;  // 10 minutes in ms
unsigned long lastRtcSync = 0;

// Helper variables
bool rtcAvailable = false;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("STARTED");
  joystick.begin(JS_SW, JS_X, JS_Y);

  display.begin(SRCLK, RCLK, SER);
  pinMode(BUZZER, OUTPUT);

  // Display startup message - need to refresh during delay
  display.setDisplayText("DL34/2416-MULTICLOCK");

  // Keep refreshing display for 2 seconds to show the text
  unsigned long startTime = millis();
  while (millis() - startTime < 2000) {
    display.refreshDisplay();
    delayMicroseconds(100);  // Small delay to prevent overwhelming the system
  }

  // Display second message
  display.setDisplayText("34163416241624162416");

  // Keep refreshing for another period
  startTime = millis();
  while (millis() - startTime < 2000) {
    display.refreshDisplay();
    delayMicroseconds(100);
  }

  // Start GPS
  gps.begin(GPS_RX);

  // Start RTC
  Wire.begin(RTC_SDA, RTC_SCL);
  if (rtc.begin()) {
    rtcAvailable = true;
    Serial.println("RTC initialized");
  } else {
    rtcAvailable = false;
    Serial.println("RTC not found! Running with GPS only...");
  }

  dht.begin();
  Serial.println("System ready...");
}

void loop() {
  display.refreshDisplay();  // Always needed for display multiplexing

  // Get joystick direction
  Serial.print("Direction: ");
  Serial.println(joystick.getDirection());
  Serial.print("Button pressed: ");
  Serial.println(joystick.getButtonPress());

  // Update GPS continuously
  gps.update();

  if (gps.hasFix()) {
    // Get Hungarian local time from GPS
    int year, month, day, dayIndex, hour, minute, second;
    gps.getHungarianTime(year, month, day, dayIndex, hour, minute, second);

    // Format time and date string: "HH:MM:SS YYYY.MM.DD."
    char timeString[21];  // 20 chars + null terminator
    sprintf(timeString, "%02d:%02d:%02d %04d.%02d.%02d.",
            hour, minute, second, year, month, day);

    // Display the formatted string
    display.setDisplayText(timeString);

    // Resync RTC every 10 minutes if available
    if (millis() - lastRtcSync >= RTC_SYNC_INTERVAL && rtcAvailable) {
      rtc.adjust(DateTime(year, month, day, hour, minute, second));
      lastRtcSync = millis();
      Serial.println("RTC synced with GPS");
    }

    // Optional: Print to serial for debugging
    Serial.printf("Displaying: %s\n", timeString);
    Serial.printf("GPS - Lat: %.6f, Lng: %.6f, Speed: %.1f km/h\n",
                  gps.getLatitude(), gps.getLongitude(), gps.getSpeedKmph());

  } else {
    // No GPS fix - show waiting message or use RTC fallback
    display.setDisplayText("WAITING FOR GPS...  ");

    Serial.println("Waiting for GPS fix...");

    if (rtcAvailable) {
      // Display RTC time as fallback
      DateTime now = rtc.now();
      char rtcTimeString[21];
      sprintf(rtcTimeString, "%02d:%02d:%02d %04d.%02d.%02d.",
              now.hour(), now.minute(), now.second(),
              now.year(), now.month(), now.day());

      // Show RTC time when GPS is unavailable
      display.setDisplayText(rtcTimeString);

      Serial.printf("RTC fallback: %s\n", rtcTimeString);
    }
  }

  /*
  // Read DHT sensor
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if (!isnan(temperature) && !isnan(humidity)) {
    Serial.printf("Temp: %.1f°C  Humidity: %.1f%%\n", temperature, humidity);
  }
  */

  delay(500);
}

void updateShiftRegisters(uint16_t data) {
  // Start sending data
  digitalWrite(RCLK, LOW);

  // Send high byte
  shiftOut(SER, SRCLK, MSBFIRST, (data >> 8) & 0xFF);

  // Send low byte
  shiftOut(SER, SRCLK, MSBFIRST, data & 0xFF);

  // Latch the data to outputs
  digitalWrite(RCLK, HIGH);
}

byte asciiToDL(char c) {
  if (c >= ' ' && c <= '_') {
    return c;
  }
  return 0x20;  // Space character
}

// Select digit (1–4) for a display
void selectDigitDataWrite(byte display, byte digit, char c) {
  uint16_t val = 0;

  // Digit select (A0/A1)
  switch (digit) {
    case 1: val |= (1UL << 14) | (1UL << 13); break;  // A0=1, A1=1
    case 2: val |= (0UL << 14) | (1UL << 13); break;  // A0=0, A1=1
    case 3: val |= (1UL << 14) | (0UL << 13); break;  // A0=1, A1=0
    case 4: val |= (0UL << 14) | (0UL << 13); break;  // A0=0, A1=0
  }

  // Data pins D0-D6
  val |= ((uint16_t)asciiToDL(c) & 0x7F) << 8;  // shift to D0-D6 bits (bit 8-14)

  // WR bit for display
  switch (display) {
    case 1: val |= (1UL << 12); break;
    case 2: val |= (1UL << 11); break;
    case 3: val |= (1UL << 10); break;
    case 4: val |= (1UL << 9); break;
    case 5: val |= (1UL << 8); break;
  }

  // Send combined value
  updateShiftRegisters(val);
  delayMicroseconds(10);

  // Clear WR
  updateShiftRegisters(val & ~(1UL << (12 - display + 1)));
}

// Set data pins (D0–D6)
void setDataPins(byte data) {
  uint16_t shiftValue = data & 0x7F;  // D0-D6
  updateShiftRegisters(shiftValue);
}

// Write character to a specific display
void writeDisplay(byte display, char c) {
  // Map display to WR bit in high byte
  uint16_t wrBit = 0;
  switch (display) {
    case 1: wrBit = (1 << 11); break;  // WR1
    case 2: wrBit = (1 << 10); break;  // WR2
    case 3: wrBit = (1 << 9); break;   // WR3
    case 4: wrBit = (1 << 8); break;   // WR4
    case 5: wrBit = (1 << 7); break;   // WR5
  }

  // Clear first
  clearDisplays();

  // Send data
  setDataPins(asciiToDL(c));

  // Pulse WR
  updateShiftRegisters(wrBit);
  delayMicroseconds(10);
  updateShiftRegisters(0);
}

// Display full text (max 20 chars, 4 per display)
void displayText(const char* message) {
  for (byte disp = 0; disp < 5; disp++) {
    for (byte digit = 0; digit < 4; digit++) {
      byte idx = disp * 4 + digit;
      char c = (message[idx] != '\0') ? message[idx] : ' ';
      selectDigitDataWrite(disp + 1, digit + 1, c);
      delayMicroseconds(50);
    }
  }
}

// Clear all displays
void clearDisplays() {
  updateShiftRegisters(1UL << 15);  // CLR high
  delayMicroseconds(10);
  updateShiftRegisters(0);  // CLR low
}