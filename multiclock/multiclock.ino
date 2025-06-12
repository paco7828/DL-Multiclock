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
#define DHT_PIN 4
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
bool showingTempHumidity = false;
int tempHumidityStep = 0;  // 0=celsius, 1=blank, 2=humidity, 3=blank, 4=return to clock
unsigned long tempHumidityStepTime = 0;

// Previous values for change detection
int prevHour = -1, prevMinute = -1, prevSecond = -1;
int prevYear = -1, prevMonth = -1, prevDay = -1;

// Sensor values
float temperature = 0.0;
float humidity = 0.0;

Rtc_Pcf8563 rtc;

// ========== Setup ==========
void setup() {
  Serial.begin(9600);
  Wire.begin();
  dht.begin();  // Initialize DHT sensor

  // Set time once
  if (EEPROM.read(SETUP_FLAG_ADDR) == 1) {
    rtc.initClock();
    rtc.setDate(12, 4, 6, 0, 25);  // day, weekday (0=Sunday), month, century (0=20xx), year (25=2025)
    rtc.setTime(13, 52, 20);       // hour, minute, second
    delay(100);
    EEPROM.write(SETUP_FLAG_ADDR, 0xFF);
    EEPROM.write(LAST_DAY_ADDR, rtc.getDay());  // Store initial day
  }

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

  displayText = " DL3416 MULTI-CLOCK";
  splitTextForDisplays();
  updateAllDisplays();
  delay(2000);

  // Show temperature and humidity on startup
  showTempHumiditySequence();

  Serial.println("Setup completed.");
}

// ========== Main Loop ==========
void loop() {
  unsigned long currentMillis = millis();

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

    // Subtract some seconds (example: subtract 10 seconds per day)
    totalSeconds -= 2;

    // Handle negative seconds
    if (totalSeconds < 0) {
      totalSeconds += 86400;  // Add 24 hours worth of seconds
      // Note: In a real implementation, you might want to handle date rollback
    }

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
  // Read temperature as Celsius
  temperature = dht.readTemperature();

  // Read humidity
  humidity = dht.readHumidity();

  // Check if any reads failed and use previous values if so
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    // Keep previous values or use defaults
    if (isnan(temperature)) temperature = 25.0;
    if (isnan(humidity)) humidity = 56.0;
  }

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print("°C, Humidity: ");
  Serial.print(humidity);
  Serial.println("%");
}

// ========== Temperature/Humidity Display Functions ==========
void showTempHumiditySequence() {
  // Read sensors before displaying
  readSensors();

  // Terminate I2C communication
  Wire.end();

  showingTempHumidity = true;
  tempHumidityStep = 0;  // Always start at step 0 (temperature)
  tempHumidityStepTime = millis();
  lastTempHumidityDisplay = millis();

  // Format temperature display with exact spacing: "  25'C   TEMPERATURE"
  char tempStr[21];
  int tempInt = (int)round(temperature);
  snprintf(tempStr, sizeof(tempStr), "  %2d'C   TEMPERATURE", tempInt);
  displayText = String(tempStr);

  splitTextForDisplays();
  updateAllDisplays();

  Serial.println("Starting temp/humidity sequence - showing temperature");
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

    case 2:  // Show humidity with exact spacing: "   56%     HUMIDITY  "
      {
        char humStr[22];
        int humInt = (int)round(humidity);
        snprintf(humStr, sizeof(humStr), "   %2d%%    HUMIDITY   ", humInt);
        displayText = String(humStr);
        splitTextForDisplays();
        updateAllDisplays();
        Serial.println("Showing humidity");
      }
      break;

    case 3:  // Blank screen after humidity
      clearDisplays();
      Serial.println("Blank screen after humidity");
      break;

    case 4:   // Return to clock
    default:  // Fallback case to ensure we always return to clock
      showingTempHumidity = false;
      tempHumidityStep = 0;

      // Re-establish I2C communication
      Wire.begin();

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
    char timeBuf[9], dateBuf[11];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", currentHour, currentMinute, currentSecond);
    snprintf(dateBuf, sizeof(dateBuf), "20%02d/%02d/%02d", currentYear, currentMonth, currentDay);

    displayText = String(timeBuf) + " " + String(dateBuf);

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

  char timeBuf[9], dateBuf[11];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", currentHour, currentMinute, currentSecond);
  snprintf(dateBuf, sizeof(dateBuf), "20%02d/%02d/%02d", currentYear, currentMonth, currentDay);

  displayText = String(timeBuf) + " " + String(dateBuf);

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

    // Re-establish I2C if needed
    Wire.begin();

    // Force clock display
    updateDisplayTextForceAll();
  }
}