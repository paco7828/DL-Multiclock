#include <Rtc_Pcf8563.h>
#include <EEPROM.h>
#include <Wire.h>
#include <DHT.h>

// ========== Configuration ==========
const byte N_OF_SEGMENTS = 4;
const byte SCREEN_COUNT = 5;

const byte CLR = A0;
const byte ADDR0 = A1;
const byte ADDR1 = A2;
const byte dataPins[7] = { 7, 8, 9, 10, 11, 12, 13 };
const byte writePins[SCREEN_COUNT] = { 2, 3, 4, 5, 6 };

// DHT11 Configuration
#define DHT_PIN A3
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// EEPROM addresses
const int SETUP_FLAG_ADDR = 0;
const int LAST_DAY_ADDR = 1;

// ========== Globals ==========
String displayText = "";
String splittedText[SCREEN_COUNT];
unsigned long lastLoopTime = 0;
unsigned long lastTempHumidityDisplay = 0;
unsigned long lastSensorRead = 0;
bool showingTempHumidity = false;
int tempHumidityStep = 0;  // 0=celsius, 1=blank, 2=humidity, 3=blank, 4=return to clock
unsigned long tempHumidityStepTime = 0;

// Previous values for change detection
int prevHour = -1, prevMinute = -1, prevSecond = -1;
int prevYear = -1, prevMonth = -1, prevDay = -1;

// Sensor values
float temperature = 25.0;  // Default values
float humidity = 50.0;
bool sensorReadingValid = false;

Rtc_Pcf8563 rtc;

// ========== Setup ==========
void setup() {
  Serial.begin(9600);
  Serial.println("Starting setup...");

  // Initialize DHT sensor
  dht.begin();
  delay(2000);

  // Initialize wire
  Wire.begin();

  // Set time once
  if (EEPROM.read(SETUP_FLAG_ADDR) == 1) {
    rtc.initClock();
    // SET DATE & TIME HERE
    rtc.setDate(12, 4, 6, 0, 25);  // day, weekday (0=Sunday), month, century (0=20xx), year (25=2025)
    rtc.setTime(15, 22, 0);        // hour, minute, second
    delay(100);
    EEPROM.write(SETUP_FLAG_ADDR, 0xFF);
    EEPROM.write(LAST_DAY_ADDR, rtc.getDay());  // Store initial day
  }

  // Set pin modes
  pinMode(CLR, OUTPUT);
  pinMode(ADDR0, OUTPUT);
  pinMode(ADDR1, OUTPUT);
  digitalWrite(CLR, HIGH);

  for (int i = 0; i < 7; i++) pinMode(dataPins[i], OUTPUT);
  for (int i = 0; i < SCREEN_COUNT; i++) {
    pinMode(writePins[i], OUTPUT);
    digitalWrite(writePins[i], HIGH);
  }

  clearDisplays();

  displayText = " DL3416 MULTI-CLOCK"; // Default text
  splitTextForDisplays();
  updateAllDisplays();
  delay(2000);

  // Read sensors
  readSensors();

  // Show temperature and humidity on startup
  showTempHumiditySequence();

  Serial.println("Setup completed.");
}

// ========== Main Loop ==========
void loop() {
  unsigned long currentMillis = millis();

  // Read sensors periodically (every 5 seconds) when not displaying temp/humidity
  if (!showingTempHumidity && (currentMillis - lastSensorRead >= 5000)) {
    readSensors();
    lastSensorRead = currentMillis;
  }

  // Handle temperature/humidity display sequence
  if (showingTempHumidity) {
    handleTempHumiditySequence(currentMillis);
    return;
  }

  // Check if it's time to show temp/humidity (every 5 minutes)
  if (currentMillis - lastTempHumidityDisplay >= 300000) {  // 300000ms = 5 minutes
    Serial.println("5 minutes elapsed - starting temp/humidity display");
    showTempHumiditySequence();
    return;
  }

  if (currentMillis - lastLoopTime < 1000) return;  // Update every second
  lastLoopTime = currentMillis;

  checkDayChange();
  updateDisplayText();
}

// ========== Day Change Check & Time Subtraction ==========
void checkDayChange() {
  int currentDay = rtc.getDay();
  int lastStoredDay = EEPROM.read(LAST_DAY_ADDR);

  if (currentDay != lastStoredDay) {
    // Day has changed, subtract seconds from current time
    int currentHour = rtc.getHour();
    int currentMinute = rtc.getMinute();
    int currentSecond = rtc.getSecond();

    // Convert current time to total seconds
    long totalSeconds = (long)currentHour * 3600 + (long)currentMinute * 60 + currentSecond;

    // Subtract second daily due to drift
    totalSeconds -= 1;

    // Convert back to hours, minutes, seconds
    int newHour = totalSeconds / 3600;
    int newMinute = (totalSeconds % 3600) / 60;
    int newSecond = totalSeconds % 60;

    // Set the new time
    rtc.setTime(newHour, newMinute, newSecond);

    // Update stored day
    EEPROM.write(LAST_DAY_ADDR, currentDay);

    Serial.println("Day changed - time adjusted");
  }
}

// ========== DHT11 Reading Function ==========
void readSensors() {
  Serial.println("Reading DHT sensor...");

  static unsigned long lastReadTime = 0;
  if (millis() - lastReadTime < 1000) {
    Serial.println("DHT read too soon, skipping...");
    return;
  }
  lastReadTime = millis();

  // Read humidity first, then temperature
  float newHum = dht.readHumidity();
  delay(250);
  float newTemp = dht.readTemperature();

  // Check if any reads failed
  if (isnan(newHum) || isnan(newTemp)) {
    Serial.println("Failed to read from DHT sensor!");
    sensorReadingValid = false;
    // Keep previous values, don't update temperature and humidity
  } else {
    // Valid reading
    temperature = newTemp;
    humidity = newHum;
    sensorReadingValid = true;

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print("°C, Humidity: ");
    Serial.print(humidity);
    Serial.println("%");
  }
}

// ========== Temperature/Humidity Display Functions ==========
void showTempHumiditySequence() {
  // Read sensors before displaying (don't end Wire communication)
  readSensors();

  showingTempHumidity = true;
  tempHumidityStep = 0;  // Always start at step 0
  tempHumidityStepTime = millis();
  lastTempHumidityDisplay = millis();

  // Format temperature display
  displayText = formatTemperatureString();

  splitTextForDisplays();
  updateAllDisplays();

  Serial.println("Starting temp/humidity sequence - showing temperature");
}

String formatTemperatureString() {
  char tempStr[21];
  if (sensorReadingValid) {
    // Display actual temperature
    snprintf(tempStr, sizeof(tempStr), " %4.1f'C TEMPERATURE ", temperature);
  } else {
    // Display error message
    snprintf(tempStr, sizeof(tempStr), " ??.?'C TEMPERATURE ");
  }
  return String(tempStr);
}

String formatHumidityString() {
  char humStr[21];
  if (sensorReadingValid) {
    // Display actual humidity
    snprintf(humStr, sizeof(humStr), "  %4.1f%%   HUMIDITY  ", humidity);
  } else {
    // Display error message
    snprintf(humStr, sizeof(humStr), "  ??.?%   HUMIDITY  ");
  }
  return String(humStr);
}

void handleTempHumiditySequence(unsigned long currentMillis) {
  // Check if enough time has passed for the current step
  if (currentMillis - tempHumidityStepTime < 2000) return;  // Each step lasts 2 seconds

  tempHumidityStepTime = currentMillis;
  tempHumidityStep++;

  Serial.print("Temp/Humidity step: ");
  Serial.println(tempHumidityStep);

  switch (tempHumidityStep) {
    case 1:  // Blank screen after temperature
      clearDisplays();
      Serial.println("Blank screen after temperature");
      break;

    case 2:  // Show humidity
      displayText = formatHumidityString();
      splitTextForDisplays();
      updateAllDisplays();
      Serial.println("Showing humidity");
      break;

    case 3:  // Blank screen after humidity
      clearDisplays();
      Serial.println("Blank screen after humidity");
      break;

    case 4:   // Return to clock
    default:  // Fallback case to ensure we always return to clock
      showingTempHumidity = false;
      tempHumidityStep = 0;

      // Reset previous values to force full update
      prevHour = prevMinute = prevSecond = -1;
      prevYear = prevMonth = prevDay = -1;

      // Force a complete display update
      updateDisplayTextForceAll();
      Serial.println("Returned to clock display");
      break;
  }
}

// ========== Time Update ==========
void updateDisplayText() {
  int currentHour = rtc.getHour();
  int currentMinute = rtc.getMinute();
  int currentSecond = rtc.getSecond();
  int currentYear = rtc.getYear();
  int currentMonth = rtc.getMonth();
  int currentDay = rtc.getDay();

  // Check what has changed
  bool timeChanged = (currentHour != prevHour || currentMinute != prevMinute || currentSecond != prevSecond);
  bool dateChanged = (currentYear != prevYear || currentMonth != prevMonth || currentDay != prevDay);

  // Only update if something changed
  if (timeChanged || dateChanged) {
    char timeBuf[9], dateBuf[12];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", currentHour, currentMinute, currentSecond);
    snprintf(dateBuf, sizeof(dateBuf), "20%02d/%02d/%02d", currentYear, currentMonth, currentDay);

    displayText = String(timeBuf) + " " + String(dateBuf) + " ";

    splitTextForDisplays();

    // Only update displays that have changed content
    updateChangedDisplays();

    // Update previous values
    prevHour = currentHour;
    prevMinute = currentMinute;
    prevSecond = currentSecond;
    prevYear = currentYear;
    prevMonth = currentMonth;
    prevDay = currentDay;

    Serial.println("Time: " + displayText);
  }
}

// ========== New Function for Force Update ==========
void updateDisplayTextForceAll() {
  int currentHour = rtc.getHour();
  int currentMinute = rtc.getMinute();
  int currentSecond = rtc.getSecond();
  int currentYear = rtc.getYear();
  int currentMonth = rtc.getMonth();
  int currentDay = rtc.getDay();

  char timeBuf[9], dateBuf[12];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", currentHour, currentMinute, currentSecond);
  snprintf(dateBuf, sizeof(dateBuf), "20%02d/%02d/%02d", currentYear, currentMonth, currentDay);

  displayText = String(timeBuf) + " " + String(dateBuf) + " ";

  splitTextForDisplays();

  // Force update of ALL displays
  updateAllDisplays();

  // Update previous values
  prevHour = currentHour;
  prevMinute = currentMinute;
  prevSecond = currentSecond;
  prevYear = currentYear;
  prevMonth = currentMonth;
  prevDay = currentDay;

  Serial.println("Time (force update): " + displayText);
}

// ========== Display Functions ==========
void splitTextForDisplays() {
  for (int i = 0; i < SCREEN_COUNT; i++) splittedText[i] = "    ";
  int len = min((int)displayText.length(), N_OF_SEGMENTS * SCREEN_COUNT);
  for (int i = 0; i < len; i++) {
    int screen = i / N_OF_SEGMENTS;
    int seg = i % N_OF_SEGMENTS;
    if (screen < SCREEN_COUNT) {
      splittedText[screen].setCharAt(seg, displayText.charAt(i));
    }
  }
}

void updateChangedDisplays() {
  static String prevSplittedText[SCREEN_COUNT];

  for (int screen = 0; screen < SCREEN_COUNT; screen++) {
    if (splittedText[screen] != prevSplittedText[screen]) {
      for (int seg = 0; seg < N_OF_SEGMENTS; seg++) {
        selectAddr(seg + 1);
        displayChar(splittedText[screen][seg], writePins[screen]);
      }
      prevSplittedText[screen] = splittedText[screen];
    }
  }
}

void updateAllDisplays() {
  for (int screen = 0; screen < SCREEN_COUNT; screen++) {
    for (int seg = 0; seg < N_OF_SEGMENTS; seg++) {
      selectAddr(seg + 1);
      displayChar(splittedText[screen][seg], writePins[screen]);
    }
  }
}

void selectAddr(byte segment) {
  digitalWrite(ADDR0, (segment == 1 || segment == 3));
  digitalWrite(ADDR1, (segment == 1 || segment == 2));
}

byte asciiToDL3416(char c) {
  return (c >= ' ' && c <= '_') ? c : 0x20;
}

void setDataPins(byte data) {
  for (int i = 0; i < 7; i++)
    digitalWrite(dataPins[i], (data >> i) & 0x01);
}

void displayChar(char c, int wrPin) {
  setDataPins(asciiToDL3416(c));
  digitalWrite(wrPin, LOW);
  delayMicroseconds(10);
  digitalWrite(wrPin, HIGH);
}

void clearDisplays() {
  digitalWrite(CLR, LOW);
  delayMicroseconds(100);
  digitalWrite(CLR, HIGH);
  delayMicroseconds(100);
}

void resetTempHumiditySequence() {
  if (showingTempHumidity) {
    Serial.println("Resetting stuck temp/humidity sequence");
    showingTempHumidity = false;
    tempHumidityStep = 0;

    // Force clock display
    updateDisplayTextForceAll();
  }
}