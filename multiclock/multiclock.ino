#include <PCF8563Clock.h>
#include <EEPROM.h>
#include <Wire.h>
#include <avr/wdt.h>

// ========== Configuration ==========
const unsigned long resetInterval = 300000UL;  // 5 minutes

const byte N_OF_SEGMENTS = 4;
const byte SCREEN_COUNT = 5;

const byte CLR = A0;
const byte ADDR0 = A1;
const byte ADDR1 = A2;
const byte dataPins[7] = { 7, 8, 9, 10, 11, 12, 13 };
const byte writePins[SCREEN_COUNT] = { 2, 3, 4, 5, 6 };

// ========== Globals ==========
String displayText = "";
String splittedText[SCREEN_COUNT];
unsigned long previousMillis = 0;
unsigned long lastLoopTime = 0;
unsigned long lastUpdateTime = 0;
String lastValidTime = "14:00:00 2025/05/30";

PCF8563Clock rtc;

// ========== Setup ==========
void setup() {
  Serial.begin(9600);
  rtc.begin();

  wdt_disable();

  if (EEPROM.read(0) == 1) {
    rtc.setTime(
      45,  // second
      42,  // minute
      16,  // hour
      7,  // day of month
      6,   // day of week - 0 = Sunday
      6,   // month
      25   // year
    );
    EEPROM.write(0, 0xFF);
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
  previousMillis = millis();

  // Enable watchdog (2s timeout)
  wdt_enable(WDTO_2S);

  Serial.println("Setup completed. Watchdog and I2C recovery enabled.");
}

// ========== Main Loop ==========
void loop() {
  unsigned long currentMillis = millis();

  // Periodic full reset every 5 minutes
  if (currentMillis - previousMillis >= resetInterval) {
    Serial.println("5 minutes elapsed. Restarting MCU...");
    Serial.flush();
    delay(100);
    wdt_enable(WDTO_15MS);
    while (1) {}
  }

  if (currentMillis - lastLoopTime < 50) return;
  lastLoopTime = currentMillis;

  updateDisplayText();
  splitTextForDisplays();
  updateAllDisplays();

  wdt_reset();  // Only pet watchdog if loop completes successfully
}

// ========== Time Update ==========
void updateDisplayText() {
  byte sec, min, hr, dom, dow, mon, yr;

  Wire.beginTransmission(0x51);
  byte error = Wire.endTransmission();

  if (error == 0) {
    rtc.getTime(sec, min, hr, dom, dow, mon, yr);
    if (hr <= 23 && min <= 59 && sec <= 59 && mon >= 1 && mon <= 12 && dom >= 1 && dom <= 31) {
      char timeBuf[9], dateBuf[11];
      snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", hr, min, sec);
      snprintf(dateBuf, sizeof(dateBuf), "20%02d/%02d/%02d", yr, mon, dom);
      displayText = String(timeBuf) + " " + String(dateBuf);
      lastValidTime = displayText;
      lastUpdateTime = millis();
      Serial.println("RTC OK: " + displayText);
      return;
    } else {
      Serial.println("Invalid RTC values");
    }
  } else {
    Serial.print("I2C Error: ");
    Serial.println(error);
  }

  // Fallback
  if (millis() - lastUpdateTime > 5000) {
    Serial.println("Using fallback time display");
    displayText = "I2C ERROR       ";
  } else {
    displayText = lastValidTime;
  }

  resetI2CBus();
}

// ========== I2C Bus Recovery ==========
void resetI2CBus() {
  Serial.println("Resetting I2C bus...");
  pinMode(A4, OUTPUT);
  pinMode(A5, OUTPUT);

  digitalWrite(A4, HIGH);
  for (int i = 0; i < 9; i++) {
    digitalWrite(A5, HIGH);
    delayMicroseconds(5);
    digitalWrite(A5, LOW);
    delayMicroseconds(5);
  }
  digitalWrite(A5, HIGH);
  digitalWrite(A4, HIGH);
  delayMicroseconds(10);

  Wire.begin();
  rtc.begin();
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
