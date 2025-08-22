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
  // Assign joystick pins
  joystick.begin(JS_SW, JS_X, JS_Y);

  // Assign shift register pins
  display.begin(SRCLK, RCLK, SER);

  // Set buzzer as output
  pinMode(BUZZER, OUTPUT);

  // Play bootup melody
  playBootupMelody();

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
  } else {
    rtcAvailable = false;
    showTemporaryMessage(" RTC NOT FOUND! :(  ");
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

  delay(20);
}

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

void handleCityModeAlerts() {
  // Only activate buzzer if city mode is enabled and we have a GPS fix
  if (!cityMode || !gps.hasFix()) {
    // Only turn off buzzer if we're not playing a melody
    if (!playingMelody) {
      digitalWrite(BUZZER, LOW);  // Turn off buzzer
      buzzerState = false;
    }
    return;
  }

  float currentSpeed = gps.getSpeedKmph();

  // Only beep if speed exceeds city limit
  if (currentSpeed <= CITY_SPEED_LIMIT) {
    // Only turn off buzzer if we're not playing a melody
    if (!playingMelody) {
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

void displayTimeMode() {
  // Handle auto-cycle for temperature and humidity display (every 10 minutes)
  if (millis() - lastAutoCycle >= AUTO_CYCLE_INTERVAL && autoCycleState == 0) {
    // Start the auto-cycle
    autoCycleState = 1;  // Show temperature
    cycleStartTime = millis();
    lastAutoCycle = millis();
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
    }
    return;  // Exit early to avoid normal time display
  }

  // Normal time display (autoCycleState == 0)
  // GPS is already updated in main loop, no need to call gps.update() again

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

    // Check for hourly chime (when seconds are 00)
    if (hour != lastHour && second == 0 && minute == 0 && !playingMelody) {
      lastHour = hour;
      playHourlyChime();
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
      static bool rtcFallbackShown = false;
      static unsigned long rtcFallbackTime = 0;

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

      // Reduce "waiting" message frequency
      static unsigned long lastWaitingMessage = 0;
      if (millis() - lastWaitingMessage >= 2000) {  // Print every 2 seconds
        lastWaitingMessage = millis();
      }
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
  } else {
    display.setDisplayText(" --.- C  TEMPERATURE");
  }
}

void displayHumidityMode() {
  // Read DHT sensor
  humidity = dht.readHumidity();

  if (!isnan(humidity)) {
    char humString[21];
    sprintf(humString, "%4.1f %%   HUMIDITY  ", humidity);
    display.setDisplayText(humString);
  } else {
    display.setDisplayText(" --.- %   HUMIDITY  ");
  }
}

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
        sprintf(statusString, "%3.0f KM/H CITY MODE %c", speed, cityMode ? 'Y' : 'N');
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
    sprintf(speedString, "%3.0f KM/H SPEED     ", speed);
    display.setDisplayText(speedString);
  } else {
    display.setDisplayText(" -- KM/H NO GPS FIX ");
  }
}

void displayLatitudeMode() {
  // GPS already updated in main loop
  if (gps.hasFix()) {
    double latitude = gps.getLatitude();
    char latString[21];
    sprintf(latString, "LAT %11.7f", latitude);
    display.setDisplayText(latString);
  } else {
    display.setDisplayText("GPS LAT  --.------- ");
  }
}

void displayLongitudeMode() {
  // GPS already updated in main loop
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

// Sound-related functions
void playBootupMelody() {
  for (byte i = 0; i < BOOTUP_MELODY_LENGTH; i++) {
    tone(BUZZER, bootupMelody[i], bootupDurations[i]);
    delay(bootupDurations[i] + 50);  // Small gap between notes
  }
  noTone(BUZZER);
}

void playHourlyChime() {
  playingMelody = true;
  melodyStartTime = millis();
  melodyStep = 0;
  tone(BUZZER, hourlyMelody[0], hourlyDurations[0]);
}

void playModeChangeBeep() {
  tone(BUZZER, MODE_CHANGE_FREQ, MODE_CHANGE_DURATION);
}

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