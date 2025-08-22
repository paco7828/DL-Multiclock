/*
(1) 74HC595:
    Control pins -> MCU:
        SRCLK -> IO3
        RCLK -> IO4
        SER -> IO5
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
        RCLK -> IO4
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
    T -> IO1
    R -> IO2

DHT11 -> MCU:
    VCC -> 5V
    GND -> GND
    DATA -> IO0

DS3231M -> MCU:
    VCC -> 5V
    GND -> GND
    VBAT -> CR2032 +
    SDA -> IO6 (pullup with 4.7k -> 3.3V)
    SCL -> IO7 (pullup with 4.7k -> 3.3V)

Buzzer -> MCU:
    + -> IO8
    - -> GND
*/

#include <DHT.h>
#include "Better-GPS.h"
#include <Wire.h>
#include "RTClib.h"

// Pins
const byte DHT_SENSOR = 0;
const byte BUZZER = 8;
const byte GPS_TX = 1;
const byte GPS_RX = 2;
const byte RTC_SDA = 6;
const byte RTC_SCL = 7;
const byte SRCLK = 3;
const byte RCLK = 4;
const byte SER = 5;

// DHT sensor
#define DHT_TYPE DHT11
DHT dht(DHT_SENSOR, DHT_TYPE);

// GPS
GPS gps;

// RTC
RTC_DS3231 rtc;

// DHT variables
float temperature = 0.0;
float humidity = 0.0;

// Sync interval
const unsigned long RTC_SYNC_INTERVAL = 10UL * 60UL * 1000UL;  // 10 minutes in ms
unsigned long lastRtcSync = 0;

// Helper variables
bool rtcAvailable = false;
uint16_t shiftData = 0;

/*
    1st bit -> displays' clear (WR)
    2nd bit -> displays' digit selector (A0)
    3rd bit -> displays' digit selector (A1)
    4th bit -> display 1's write (WR)
    5th bit -> display 2's write (WR)
    6th bit -> display 3's write (WR)
    7th bit -> display 4's write (WR)
    8th bit -> display 5's write (WR)
    9th bit -> displays' data pin (D0)
    10th bit -> displays' data pin (D1)
    11th bit -> displays' data pin (D2)
    12th bit -> displays' data pin (D3)
    13th bit -> displays' data pin (D4)
    14th bit -> displays' data pin (D5)
    15th bit -> displays' data pin (D6)
    16th bit -> nothing

    10000000-00000000 -> Clears Displays
    00010000-00000000 -> Write Display 1
    00001000-00000000 -> Write Display 2
    00000100-00000000 -> Write Display 3
    00000010-00000000 -> Write Display 4
    00000001-00000000 -> Write Display 5
    01100000-00000000 -> Select 1st digit
    00100000-00000000 -> Select 2nd digit
    01000000-00000000 -> Select 3rd digit
    00000000-00000000 -> Select 4th digit
    00000000-10000000 -> D0 activation
    00000000-01000000 -> D1 activation
    00000000-00100000 -> D2 activation
    00000000-00010000 -> D3 activation
    00000000-00001000 -> D4 activation
    00000000-00000100 -> D5 activation
    00000000-00000010 -> D6 activation
*/

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("STARTED");
  pinMode(BUZZER, OUTPUT);
  pinMode(SRCLK, OUTPUT);
  pinMode(RCLK, OUTPUT);
  pinMode(SER, OUTPUT);

  // Start GPS
  gps.begin(GPS_RX, GPS_TX);

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

  displayText("HELLO WORLD 12345");  // Up to 20 chars
}

void loop() {
  gps.update();

  if (gps.hasFix()) {
    // Resync RTC every 10 minutes
    if (millis() - lastRtcSync >= RTC_SYNC_INTERVAL && rtcAvailable) {
      rtc.adjust(DateTime(gps.getYear(), gps.getMonth(), gps.getDay(),
                          gps.getHour(), gps.getMinute(), gps.getSecond()));
      lastRtcSync = millis();
      Serial.println("RTC synced with GPS");
    }

    // Print GPS info
    Serial.print("Lat: ");
    Serial.print(gps.getLatitude(), 6);
    Serial.print("  Lng: ");
    Serial.print(gps.getLongitude(), 6);

    Serial.print("  Speed: ");
    Serial.print(gps.getSpeedKmph());
    Serial.println(" km/h");

    // Hungarian local time
    int year, month, day, dayIndex, hour, minute, second;
    gps.getHungarianTime(year, month, day, dayIndex, hour, minute, second);

    Serial.printf("Hungarian Time: %04d-%02d-%02d (DayIndex=%d) %02d:%02d:%02d\n",
                  year, month, day, dayIndex, hour, minute, second);
  } else {
    Serial.println("Waiting for GPS fix...");

    if (rtcAvailable) {
      // Fallback to RTC when no GPS
      DateTime now = rtc.now();
      Serial.printf("RTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
                    now.year(), now.month(), now.day(),
                    now.hour(), now.minute(), now.second());
    }
  }

  // Read DHT
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if (!isnan(temperature) && !isnan(humidity)) {
    Serial.printf("Temp: %.1f °C  Humidity: %.1f %%\n", temperature, humidity);
  }

  delay(1000);
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