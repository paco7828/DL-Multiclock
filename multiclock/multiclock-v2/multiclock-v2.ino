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
unsigned long lastDHTRead = 0;
const unsigned long DHT_READ_INTERVAL = 2000;  // Read DHT every 2 seconds when needed

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

// City Mode variables
bool cityMode = false;  // City mode state (Y/N)
unsigned long lastBuzzerBeep = 0;
bool buzzerState = false;
const float CITY_SPEED_LIMIT = 50.0;  // Speed limit for city mode in km/h

// Sound variables
int lastHour = -1;  // Track last displayed hour for hourly chime
bool playingMelody = false;
unsigned long melodyStartTime = 0;
byte melodyStep = 0;

// Bootup melody variables
bool playingBootupMelody = false;
unsigned long bootupMelodyStartTime = 0;
byte bootupMelodyStep = 0;

// Bootup melody notes (frequencies in Hz)
const int bootupMelody[] = { 523, 659, 784, 1047 };    // C5, E5, G5, C6
const int bootupDurations[] = { 150, 150, 150, 300 };  // Note durations in ms
const byte BOOTUP_MELODY_LENGTH = 4;

// Hourly chime melody notes
const int hourlyMelody[] = { 1047, 784, 523 };    // C6, G5, C5
const int hourlyDurations[] = { 200, 200, 400 };  // Note durations in ms
const byte HOURLY_MELODY_LENGTH = 3;

// Mode change beep
const int MODE_CHANGE_FREQ = 800;      // Frequency for mode change beep
const int MODE_CHANGE_DURATION = 100;  // Duration for mode change beep

// Variable to store selected mode
byte currentMode = 0;      // Default mode
const byte MAX_MODES = 6;  // Total number of modes

// Joystick navigation variables
bool joystickCentered = true;  // Track if joystick returned to center

// Startup display variables
bool showingStartupMessages = true;
byte startupMessageStep = 0;
unsigned long startupMessageTime = 0;
const unsigned long STARTUP_MESSAGE_DURATION = 2000;  // 2 seconds per message

// Message display control
unsigned long messageStartTime = 0;
const unsigned long MESSAGE_DISPLAY_DURATION = 3000;  // 3 seconds
bool showingTemporaryMessage = false;

// Helper variables
bool rtcAvailable = false;
byte joystickDirection = 0;
byte joystickBtnPressed = 0;

void setup() {
  // Assign joystick pins
  joystick.begin(JS_SW, JS_X, JS_Y);

  // Assign shift register pins
  display.begin(SRCLK, RCLK, SER);

  // Set buzzer as output
  pinMode(BUZZER, OUTPUT);

  // Start bootup melody
  startBootupMelody();

  // Start startup message sequence
  display.setDisplayText("DL34/2416-MULTICLOCK");
  startupMessageTime = millis();
  startupMessageStep = 0;

  // Start GPS
  gps.begin(GPS_RX);

  // Start RTC
  Wire.begin(RTC_SDA, RTC_SCL);
  if (rtc.begin()) {
    rtcAvailable = true;
  } else {
    rtcAvailable = false;
  }

  dht.begin();
}

void loop() {
  display.refreshDisplay();  // Always needed for display multiplexing

  // Handle startup sequence
  if (showingStartupMessages) {
    handleStartupSequence();
    handleBootupMelody();
    return;
  }

  // Check if we should stop showing temporary message
  if (showingTemporaryMessage && (millis() - messageStartTime >= MESSAGE_DISPLAY_DURATION)) {
    showingTemporaryMessage = false;
  }

  // Skip normal display logic if showing temporary message
  if (showingTemporaryMessage) {
    delay(10);  // Reduced delay when showing temporary message
    return;
  }

  // Update GPS only once per loop
  gps.update();

  // Get joystick values
  joystickDirection = joystick.getDirection();
  joystickBtnPressed = joystick.getButtonPress();

  // Handle joystick navigation with debouncing
  handleJoystickNavigation();

  // Handle melody playback
  handleMelodyPlayback();

  // Handle city mode buzzer logic (only if not playing melody)
  if (!playingMelody) {
    handleCityModeAlerts();
  }

  // Mode handling
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

  delay(50);
}

// Function to handle startup message sequence
void handleStartupSequence() {
  if (millis() - startupMessageTime >= STARTUP_MESSAGE_DURATION) {
    startupMessageStep++;

    if (startupMessageStep == 1) {
      // Show second message
      display.setDisplayText("34163416241624162416");
      startupMessageTime = millis();
    } else if (startupMessageStep == 2) {
      // Show RTC status message
      if (rtcAvailable) {
        showTemporaryMessage(" RTC IS INITIALIZED ");
      } else {
        showTemporaryMessage(" RTC NOT FOUND! :(  ");
      }
      showingStartupMessages = false;  // End startup sequence
    }
  }
}

// Function to start bootup melody
void startBootupMelody() {
  playingBootupMelody = true;
  bootupMelodyStartTime = millis();
  bootupMelodyStep = 0;
  tone(BUZZER, bootupMelody[0], bootupDurations[0]);
}

// Function to handle bootup melody playback (non-blocking)
void handleBootupMelody() {
  if (!playingBootupMelody) return;

  // Check if current note duration has passed
  if (millis() - bootupMelodyStartTime >= bootupDurations[bootupMelodyStep] + 50) {  // +50 for gap
    bootupMelodyStep++;

    if (bootupMelodyStep < BOOTUP_MELODY_LENGTH) {
      // Play next note
      bootupMelodyStartTime = millis();
      tone(BUZZER, bootupMelody[bootupMelodyStep], bootupDurations[bootupMelodyStep]);
    } else {
      // Melody finished
      playingBootupMelody = false;
      noTone(BUZZER);
    }
  }
}

// Function to read DHT sensor (only when needed and with rate limiting)
void readDHTSensor() {
  if (millis() - lastDHTRead >= DHT_READ_INTERVAL) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    lastDHTRead = millis();
  }
}

// Function to handle joystick navigation with debounce
void handleJoystickNavigation() {
  // Check if joystick returned to center (direction == 0)
  if (joystickDirection == 0) {
    joystickCentered = true;
  }

  // Only process navigation if joystick was centered and now moved
  if (joystickCentered && joystickDirection != 0) {
    if (joystickDirection == 2 || joystickDirection == 4) {  // RIGHT or TOP - move forward in modes
      currentMode = (currentMode + 1) % MAX_MODES;
      joystickCentered = false;  // Prevent repeated navigation

      // Play mode change beep
      playModeChangeBeep();

      // Reset auto-cycle when leaving mode 0
      if (currentMode == 1) {
        autoCycleState = 0;
      }
    } else if (joystickDirection == 1 || joystickDirection == 3) {  // LEFT or BOTTOM - move backward in modes
      currentMode = (currentMode == 0) ? (MAX_MODES - 1) : (currentMode - 1);
      joystickCentered = false;  // Prevent repeated navigation

      // Play mode change beep
      playModeChangeBeep();

      // Reset auto-cycle when entering mode 0 from another mode
      if (currentMode == 0) {
        autoCycleState = 0;
        lastAutoCycle = millis();  // Reset the 10-minute timer
      }
    }
  }
}

// Function to handle city mode with buzzer
void handleCityModeAlerts() {
  // Only activate buzzer if city mode is enabled and we have a GPS fix
  if (!cityMode || !gps.hasFix()) {
    // Only turn off buzzer if we're not playing a melody
    if (!playingMelody && !playingBootupMelody) {
      digitalWrite(BUZZER, LOW);  // Turn off buzzer
      buzzerState = false;
    }
    return;
  }

  float currentSpeed = gps.getSpeedKmph();

  // Only beep if speed exceeds city limit
  if (currentSpeed <= CITY_SPEED_LIMIT) {
    // Only turn off buzzer if we're not playing a melody
    if (!playingMelody && !playingBootupMelody) {
      digitalWrite(BUZZER, LOW);  // Turn off buzzer
      buzzerState = false;
    }
    return;
  }

  // Calculate beep interval based on speed
  // Speed range: 50-100+ km/h
  // Beep interval range: 1000ms (slow) to 100ms (very fast)
  unsigned long beepInterval;
  if (currentSpeed >= 100.0) {
    beepInterval = 100;  // Very fast beeping for 100+ km/h
  } else {
    // Linear interpolation: 50 km/h = 1000ms, 100 km/h = 100ms
    beepInterval = 1000 - ((currentSpeed - CITY_SPEED_LIMIT) / 50.0) * 900;
  }

  // Handle buzzer beeping
  if (millis() - lastBuzzerBeep >= beepInterval) {
    buzzerState = !buzzerState;
    digitalWrite(BUZZER, buzzerState ? HIGH : LOW);
    lastBuzzerBeep = millis();
  }
}

// Function to display time and date (default)
void displayTimeMode() {
  // Handle auto-cycle for temperature and humidity display (every 10 minutes)
  if (millis() - lastAutoCycle >= AUTO_CYCLE_INTERVAL && autoCycleState == 0) {
    // Start the auto-cycle
    autoCycleState = 1;  // Show temperature
    cycleStartTime = millis();
    lastAutoCycle = millis();
    readDHTSensor();  // Read DHT for auto-cycle
  }

  // Handle auto-cycle states
  if (autoCycleState == 1) {  // Showing temperature
    // Valid temperature
    if (!isnan(temperature)) {
      char tempString[21];
      sprintf(tempString, "%4.1f C  TEMPERATURE", temperature);
      display.setDisplayText(tempString);
    }
    // Invalid temperature
    else {
      display.setDisplayText(" --.- C  TEMPERATURE");
    }

    // Check if 4 seconds have passed, move to humidity
    if (millis() - cycleStartTime >= TEMP_DISPLAY_DURATION) {
      autoCycleState = 2;  // Show humidity
      cycleStartTime = millis();
    }
    return;  // Exit early to avoid normal time display
  }

  if (autoCycleState == 2) {  // Showing humidity
    // Valid humidity
    if (!isnan(humidity)) {
      char humString[21];
      sprintf(humString, "%4.1f %%   HUMIDITY  ", humidity);
      display.setDisplayText(humString);
    }
    // Invalid humidity
    else {
      display.setDisplayText(" --.- %   HUMIDITY  ");
    }

    // Check if 4 seconds have passed, return to normal time
    if (millis() - cycleStartTime >= HUMIDITY_DISPLAY_DURATION) {
      autoCycleState = 0;  // Return to normal time display
    }
    return;  // Exit early to avoid normal time display
  }

  // Static variables for RTC fallback handling
  static bool rtcFallbackShown = false;
  static unsigned long rtcFallbackTime = 0;

  // Normal time display (autoCycleState == 0)
  if (gps.hasFix()) {
    // Reset RTC fallback flag when GPS fix is regained
    rtcFallbackShown = false;

    // Get Hungarian local time from GPS
    int year, month, day, dayIndex, hour, minute, second;
    gps.getHungarianTime(year, month, day, dayIndex, hour, minute, second);

    // Format time and date string: "HH:MM:SS YYYY.MM.DD."
    char timeString[21];  // 20 chars + null terminator
    sprintf(timeString, "%02d:%02d:%02d %04d.%02d.%02d.",
            hour, minute, second, year, month, day);

    // Display the formatted string
    display.setDisplayText(timeString);

    // Check for hourly chime (when seconds and minutes are 00)
    if (hour != lastHour && second == 0 && minute == 0 && !playingMelody) {
      lastHour = hour;
      playingMelody = true;
      melodyStartTime = millis();
      melodyStep = 0;
      tone(BUZZER, hourlyMelody[0], hourlyDurations[0]);
    }

    // Resync RTC every 10 minutes if available
    if (millis() - lastRtcSync >= RTC_SYNC_INTERVAL && rtcAvailable) {
      rtc.adjust(DateTime(year, month, day, hour, minute, second));
      lastRtcSync = millis();
      showTemporaryMessage("SYNCED!  RTC BY GPS ");
      return;  // Exit loop to show sync message
    }

  } else {
    // No GPS fix
    if (rtcAvailable) {
      // Show RTC fallback message for 3 seconds, then display RTC time
      if (!rtcFallbackShown) {
        showTemporaryMessage("   RTC    FALLBACK  ");
        rtcFallbackShown = true;
        rtcFallbackTime = millis();
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
      }
    } else {
      // No GPS fix and no RTC available
      display.setDisplayText("WAITING FOR GPS...  ");
    }
  }
}

// Function to show temperature
void displayTemperatureMode() {
  // Read DHT sensor only when in this mode
  readDHTSensor();

  // Valid temperature
  if (!isnan(temperature)) {
    char tempString[21];
    sprintf(tempString, "%4.1f C  TEMPERATURE", temperature);
    display.setDisplayText(tempString);
  }
  // Invalid temperature
  else {
    display.setDisplayText(" --.- C  TEMPERATURE");
  }
}

// Function to display humidity
void displayHumidityMode() {
  // Read DHT sensor only when in this mode
  readDHTSensor();

  // Valid humidity
  if (!isnan(humidity)) {
    char humString[21];
    sprintf(humString, "%4.1f %%   HUMIDITY  ", humidity);
    display.setDisplayText(humString);
  }
  // Invalid humidity
  else {
    display.setDisplayText(" --.- %   HUMIDITY  ");
  }
}

// Function to display current speed
void displaySpeedMode() {
  // Handle joystick button press to toggle city mode and show status
  static bool buttonPressed = false;
  static unsigned long buttonPressTime = 0;
  static bool showingCityModeStatus = false;

  if (joystickBtnPressed && !buttonPressed) {
    // Button just pressed - toggle city mode
    playModeChangeBeep();
    cityMode = !cityMode;
    buttonPressed = true;
    buttonPressTime = millis();
    showingCityModeStatus = true;

    // Turn off buzzer when toggling mode
    digitalWrite(BUZZER, LOW);
    buzzerState = false;
  } else if (!joystickBtnPressed) {
    buttonPressed = false;
  }

  // Show city mode status for 3 seconds after button press
  if (showingCityModeStatus) {
    if (millis() - buttonPressTime >= MESSAGE_DISPLAY_DURATION) {
      showingCityModeStatus = false;
    } else {
      // Display city mode status
      if (gps.hasFix()) {
        float speed = gps.getSpeedKmph();
        char statusString[21];
        if (speed >= 100.0) {
          sprintf(statusString, "%.0f KM/H CITY MODE %c", speed, cityMode ? 'Y' : 'N');
        } else {
          sprintf(statusString, "%3.0f KM/H CITY MODE %c", speed, cityMode ? 'Y' : 'N');
        }
        display.setDisplayText(statusString);
      } else {
        char statusString[21];
        sprintf(statusString, " -- KM/H CITY MODE %c", cityMode ? 'Y' : 'N');
        display.setDisplayText(statusString);
      }
      return;
    }
  }

  // Normal speed display
  if (gps.hasFix()) {
    float speed = gps.getSpeedKmph();
    char speedString[21];
    if (speed >= 100.0) {
      // For 3-digit speeds: "102 KM/H SPEED     "
      sprintf(speedString, "%.0f KM/H SPEED     ", speed);
    } else {
      // For 2-digit speeds: " 99 KM/H SPEED     "
      sprintf(speedString, "%3.0f KM/H SPEED     ", speed);
    }
    display.setDisplayText(speedString);
  } else {
    display.setDisplayText(" -- KM/H NO GPS FIX ");
  }
}

// Function to display GPS latitude
void displayLatitudeMode() {
  if (gps.hasFix()) {
    double latitude = gps.getLatitude();
    char latString[21];
    sprintf(latString, "LAT %11.7f", latitude);
    display.setDisplayText(latString);
  } else {
    display.setDisplayText("GPS LAT  --.------- ");
  }
}

// Function to display GPS longitude
void displayLongitudeMode() {
  if (gps.hasFix()) {
    double longitude = gps.getLongitude();
    char lonString[21];
    sprintf(lonString, "LON %11.7f", longitude);
    display.setDisplayText(lonString);
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

// Function to handle hourly melody playback
void handleMelodyPlayback() {
  if (!playingMelody) return;

  // Check if current note duration has passed
  if (millis() - melodyStartTime >= hourlyDurations[melodyStep]) {
    melodyStep++;

    if (melodyStep < HOURLY_MELODY_LENGTH) {
      // Play next note
      melodyStartTime = millis();
      tone(BUZZER, hourlyMelody[melodyStep], hourlyDurations[melodyStep]);
    } else {
      // Melody finished
      playingMelody = false;
      noTone(BUZZER);
    }
  }
}

// Beep function which plays when switching modes
void playModeChangeBeep() {
  tone(BUZZER, MODE_CHANGE_FREQ, MODE_CHANGE_DURATION);
}