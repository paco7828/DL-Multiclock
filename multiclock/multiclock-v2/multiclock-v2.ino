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

// Auto-display cycle for mode 0 (every 10 minutes)
const unsigned long AUTO_CYCLE_INTERVAL = 10UL * 60UL * 1000UL;  // 10 minutes in ms
const unsigned long TEMP_DISPLAY_DURATION = 4000UL;              // 4 seconds
const unsigned long HUMIDITY_DISPLAY_DURATION = 4000UL;          // 4 seconds
unsigned long lastAutoCycle = 0;
unsigned long cycleStartTime = 0;
byte autoCycleState = 0;  // 0 = normal time, 1 = showing temp, 2 = showing humidity

// Modes
char* MODE_TITLES[] = {
  "HH:MM:SS YYYY.MM.DD.",
  " --.- C  TEMPERATURE",
  " --.- %   HUMIDITY  ",
  " XX KM/H CITY MODE Y",
  "GPS LAT  --.------- ",
  "GPS LON  --.------- "
};

// Variable to store selected mode
byte currentMode = 0;      // Default mode
const byte MAX_MODES = 6;  // Total number of modes

// Joystick navigation variables
byte lastJoystickDirection = 0;
bool joystickCentered = true;  // Track if joystick returned to center

// 0 -> "16:00:00 2025.06.06."
// 1 -> " 24.5 C  TEMPERATURE"
// 2 ->" 32.5 %   HUMIDITY  "
// 3 -> " XX KM/H" (when button pressed -> " XX KM/H CITY MODE Y" or " XX KM/H CITY MODE N", shown for 3 seconds)
// 4 -> "GPS LAT  18.1234567 "
// 5 -> "GPS LON  18.1234567 "

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

  // Handle joystick navigation with debouncing
  handleJoystickNavigation();

  // SHOW TEMPERATURE for 4 seconds => SHOW HUMIDITY for 4 seconds => JUMP BACK TO CURRENT MODE (every 10 minutes)
  /*
     1;  // LEFT
     2;   // RIGHT
     3;  // BOTTOM
     4;   // TOP
  */
  switch (currentMode) {
    case 0:
      displayTimeMode();
      break;
    case 1:
      displayTemperatureMode();
      break;
    case 2:
      displayHumidityMode();
      break;
    case 3:
      displaySpeedMode();
      break;
    case 4:
      displayLatitudeMode();
      break;
    case 5:
      displayLongitudeMode();
      break;
  }

  delay(500);
}

void handleJoystickNavigation() {
  // Check if joystick returned to center (direction == 0)
  if (joystickDirection == 0) {
    joystickCentered = true;
  }

  // Only process navigation if joystick was centered and now moved
  if (joystickCentered && joystickDirection != 0) {
    if (joystickDirection == 2) {  // RIGHT - move forward in modes
      currentMode = (currentMode + 1) % MAX_MODES;
      joystickCentered = false;  // Prevent repeated navigation

      // Reset auto-cycle when leaving mode 0
      if (currentMode == 1) {
        autoCycleState = 0;
      }

      Serial.printf("Mode changed to: %d\n", currentMode);
    } else if (joystickDirection == 1) {  // LEFT - move backward in modes
      currentMode = (currentMode == 0) ? (MAX_MODES - 1) : (currentMode - 1);
      joystickCentered = false;  // Prevent repeated navigation

      // Reset auto-cycle when entering mode 0 from another mode
      if (currentMode == 0) {
        autoCycleState = 0;
        lastAutoCycle = millis();  // Reset the 10-minute timer
      }

      Serial.printf("Mode changed to: %d\n", currentMode);
    }
  }
}

void displayTimeMode() {
  // Handle auto-cycle for temperature and humidity display (every 10 minutes)
  if (millis() - lastAutoCycle >= AUTO_CYCLE_INTERVAL && autoCycleState == 0) {
    // Start the auto-cycle
    autoCycleState = 1;  // Show temperature
    cycleStartTime = millis();
    lastAutoCycle = millis();
    Serial.println("Starting auto-cycle: showing temperature");
  }

  // Handle auto-cycle states
  if (autoCycleState == 1) {  // Showing temperature
    // Read and display temperature
    temperature = dht.readTemperature();

    if (!isnan(temperature)) {
      char tempString[21];
      sprintf(tempString, "%4.1f C  TEMPERATURE", temperature);
      display.setDisplayText(tempString);
    } else {
      display.setDisplayText(" --.- C  TEMPERATURE");
    }

    // Check if 4 seconds have passed, move to humidity
    if (millis() - cycleStartTime >= TEMP_DISPLAY_DURATION) {
      autoCycleState = 2;  // Show humidity
      cycleStartTime = millis();
      Serial.println("Auto-cycle: switching to humidity");
    }
    return;  // Exit early to avoid normal time display
  }

  if (autoCycleState == 2) {  // Showing humidity
    // Read and display humidity
    humidity = dht.readHumidity();

    if (!isnan(humidity)) {
      char humString[21];
      sprintf(humString, "%4.1f %%   HUMIDITY  ", humidity);
      display.setDisplayText(humString);
    } else {
      display.setDisplayText(" --.- %   HUMIDITY  ");
    }

    // Check if 4 seconds have passed, return to normal time
    if (millis() - cycleStartTime >= HUMIDITY_DISPLAY_DURATION) {
      autoCycleState = 0;  // Return to normal time display
      Serial.println("Auto-cycle: returning to time display");
    }
    return;  // Exit early to avoid normal time display
  }

  // Normal time display (autoCycleState == 0)
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
}

void displayTemperatureMode() {
  // Read DHT sensor
  temperature = dht.readTemperature();

  if (!isnan(temperature)) {
    char tempString[21];
    sprintf(tempString, "%4.1f C  TEMPERATURE", temperature);
    display.setDisplayText(tempString);
    Serial.printf("Temp: %.1f°C\n", temperature);
  } else {
    display.setDisplayText(" --.- C  TEMPERATURE");
    Serial.println("Temperature reading failed");
  }
}

void displayHumidityMode() {
  // Read DHT sensor
  humidity = dht.readHumidity();

  if (!isnan(humidity)) {
    char humString[21];
    sprintf(humString, "%4.1f %%   HUMIDITY  ", humidity);
    display.setDisplayText(humString);
    Serial.printf("Humidity: %.1f%%\n", humidity);
  } else {
    display.setDisplayText(" --.- %   HUMIDITY  ");
    Serial.println("Humidity reading failed");
  }
}

void displaySpeedMode() {
  gps.update();

  if (gps.hasFix()) {
    float speed = gps.getSpeedKmph();
    char speedString[21];
    sprintf(speedString, "%3.0f KM/H SPEED     ", speed);
    display.setDisplayText(speedString);
    Serial.printf("Speed: %.1f km/h\n", speed);
  } else {
    display.setDisplayText(" -- KM/H NO GPS FIX ");
  }
}

void displayLatitudeMode() {
  gps.update();

  if (gps.hasFix()) {
    double latitude = gps.getLatitude();
    char latString[21];
    sprintf(latString, "LAT %11.7f", latitude);
    display.setDisplayText(latString);
    Serial.printf("Latitude: %.7f\n", latitude);
  } else {
    display.setDisplayText("GPS LAT  --.------- ");
  }
}

void displayLongitudeMode() {
  gps.update();

  if (gps.hasFix()) {
    double longitude = gps.getLongitude();
    char lonString[21];
    sprintf(lonString, "LON %11.7f", longitude);
    display.setDisplayText(lonString);
    Serial.printf("Longitude: %.7f\n", longitude);
  } else {
    display.setDisplayText("GPS LON  --.------- ");
  }
}

// Function to show a temporary message for 3 seconds
void showTemporaryMessage(const char* message) {
  display.setDisplayText(message);
  messageStartTime = millis();
  showingTemporaryMessage = true;
}