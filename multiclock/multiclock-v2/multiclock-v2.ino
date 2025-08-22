#include "DL-display.h"
#include <Wire.h>
#include "RTClib.h"
#include <DHT.h>
#include "Better-GPS.h"
#include "Better-JoyStick.h"

// Pins
// Shift registers
const byte SRCLK = 3;
const byte RCLK = 10;
const byte SER = 9;

// Joystick
const byte JS_SW = 1;
const byte JS_X = 0;
const byte JS_Y = 2;

// DHT11/22 sensor
const byte DHT_SENSOR = 5;

// Buzzer
const byte BUZZER = 8;

// GPS
const byte GPS_RX = 4;

// RTC
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

// Message display control
unsigned long messageStartTime = 0;
const unsigned long MESSAGE_DISPLAY_DURATION = 3000;  // 3 seconds
bool showingTemporaryMessage = false;

// Helper variables
bool rtcAvailable = false;
byte joystickDirection = 0;
byte joystickBtnPressed = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Assign joystick pins
  joystick.begin(JS_SW, JS_X, JS_Y);

  // Assign shift register pins
  display.begin(SRCLK, RCLK, SER);

  // Set buzzer as output
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
    showTemporaryMessage(" RTC IS INITIALIZED ");
    Serial.println("RTC initialized");
  } else {
    rtcAvailable = false;
    showTemporaryMessage(" RTC NOT FOUND! :(  ");
    Serial.println("RTC not found! Running with GPS only...");
  }

  dht.begin();
}

void loop() {
  display.refreshDisplay();  // Always needed for display multiplexing

  // Check if we should stop showing temporary message
  if (showingTemporaryMessage && (millis() - messageStartTime >= MESSAGE_DISPLAY_DURATION)) {
    showingTemporaryMessage = false;
  }

  // Skip normal display logic if showing temporary message
  if (showingTemporaryMessage) {
    delay(50);  // Short delay when showing temporary message
    return;
  }

  // Get joystick values
  joystickDirection = joystick.getDirection();
  joystickBtnPressed = joystick.getButtonPress();

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
      showTemporaryMessage("SYNCED!  RTC BY GPS ");
      Serial.println("RTC synced with GPS");
      return;  // Exit loop to show sync message
    }

    // Optional: Print to serial for debugging
    Serial.printf("Displaying: %s\n", timeString);
    Serial.printf("GPS - Lat: %.6f, Lng: %.6f, Speed: %.1f km/h\n",
                  gps.getLatitude(), gps.getLongitude(), gps.getSpeedKmph());

  } else {
    // No GPS fix
    if (rtcAvailable) {
      // Show RTC fallback message for 3 seconds, then display RTC time
      static bool rtcFallbackShown = false;
      static unsigned long rtcFallbackTime = 0;

      if (!rtcFallbackShown) {
        showTemporaryMessage("   RTC    FALLBACK  ");
        rtcFallbackShown = true;
        rtcFallbackTime = millis();
        Serial.println("Showing RTC fallback message");
        return;
      }

      // After showing fallback message, display RTC time
      if (millis() - rtcFallbackTime >= MESSAGE_DISPLAY_DURATION) {
        DateTime now = rtc.now();
        char rtcTimeString[21];
        sprintf(rtcTimeString, "%02d:%02d:%02d %04d.%02d.%02d.",
                now.hour(), now.minute(), now.second(),
                now.year(), now.month(), now.day());

        display.setDisplayText(rtcTimeString);
        Serial.printf("RTC fallback: %s\n", rtcTimeString);
      }
    } else {
      // No GPS fix and no RTC available
      display.setDisplayText("WAITING FOR GPS...  ");
      Serial.println("Waiting for GPS fix...");
    }
  }

  /*
  // Read DHT sensor
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if (!isnan(temperature) && !isnan(humidity)) {
    Serial.printf("Temp: %.1fÂ°C  Humidity: %.1f%%\n", temperature, humidity);
  }
  */

  delay(500);
}

// Function to show a temporary message for 3 seconds
void showTemporaryMessage(const char* message) {
  display.setDisplayText(message);
  messageStartTime = millis();
  showingTemporaryMessage = true;
}