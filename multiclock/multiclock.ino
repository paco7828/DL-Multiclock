#include <Rtc_Pcf8563.h>
#include <EEPROM.h>
#include <Wire.h>

// ========== Configuration ==========
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
unsigned long lastLoopTime = 0;

Rtc_Pcf8563 rtc;

// ========== Setup ==========
void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Set time once
  if (EEPROM.read(0) == 1) {
    rtc.initClock();
    rtc.setDate(8, 1, 6, 0, 25);  // day, weekday (0=Sunday), month, century (0=20xx), year (25=2025)
    rtc.setTime(16, 6, 0);        // hour, minute, second
    delay(100);
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

  displayText = "TIME SETREADY TO USE";
  splitTextForDisplays();
  updateAllDisplays();
  delay(2000);
  Serial.println("Setup completed.");
}

// ========== Main Loop ==========
void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastLoopTime < 1000) return;  // Update every second
  lastLoopTime = currentMillis;

  updateDisplayText();
  splitTextForDisplays();
  updateAllDisplays();
}

// ========== Time Update ==========
void updateDisplayText() {
  char timeBuf[9], dateBuf[11];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", rtc.getHour(), rtc.getMinute(), rtc.getSecond());
  snprintf(dateBuf, sizeof(dateBuf), "20%02d/%02d/%02d", rtc.getYear(), rtc.getMonth(), rtc.getDay());

  displayText = String(timeBuf) + " " + String(dateBuf);

  Serial.println("Time: " + displayText);
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