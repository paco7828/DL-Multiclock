#include <PCF8563Clock.h>

const byte N_OF_SEGMENTS = 4;
const byte SCREEN_COUNT = 5;

// Shared pins
const byte CLR = A0;
const byte ADDR0 = A1;
const byte ADDR1 = A2;
const byte dataPins[7] = { 7, 8, 9, 10, 11, 12, 13 };

// Write pins for each display
const byte writePins[SCREEN_COUNT] = { 2, 3, 4, 5, 6 };

String text = "";
String splittedText[SCREEN_COUNT];

PCF8563Clock rtc;

void setup() {
  Serial.begin(9600);
  rtc.begin();

  // Set the initial time
  rtc.setTime(0, 56, 23, 28, 5, 3, 25); // 23:56:00 on March 28, 2024

  // Initialize pins
  pinMode(CLR, OUTPUT);
  pinMode(ADDR0, OUTPUT);
  pinMode(ADDR1, OUTPUT);
  digitalWrite(CLR, HIGH);

  // Initialize shared data pins
  for (int i = 0; i < 7; i++) {
    pinMode(dataPins[i], OUTPUT);
  }

  // Initialize write pins
  for (int i = 0; i < SCREEN_COUNT; i++) {
    pinMode(writePins[i], OUTPUT);
    digitalWrite(writePins[i], HIGH);
  }

  // Clear displays
  clearDisplays();
}

void loop() {
  // Update time and text every second
    updateTime();

    // Split text safely
    int count = 0;
    for (int i = 0; i < text.length() && count < SCREEN_COUNT; i += N_OF_SEGMENTS) {
      splittedText[count] = text.substring(i, min(i + N_OF_SEGMENTS, text.length()));

      // Pad with spaces if needed
      while (splittedText[count].length() < N_OF_SEGMENTS) {
        splittedText[count] = " " + splittedText[count];
      }

      count++;
    }

  // Iterate through each display
  for (int screenIndex = 0; screenIndex < SCREEN_COUNT; screenIndex++) {
    // Iterate through each segment of the text for this display
    for (int segmentIndex = 0; segmentIndex < N_OF_SEGMENTS; segmentIndex++) {
      // Select the correct segment
      selectAddr(segmentIndex + 1);

      // Display the character
      char currentChar = splittedText[screenIndex][segmentIndex];
      displayChar(currentChar, writePins[screenIndex]);
    }
  }
}

void selectAddr(byte segment) {
  switch (segment) {
    case 1:  // First segment (top)
      digitalWrite(ADDR0, HIGH);
      digitalWrite(ADDR1, HIGH);
      break;
    case 2:  // Second segment
      digitalWrite(ADDR0, LOW);
      digitalWrite(ADDR1, HIGH);
      break;
    case 3:  // Third segment
      digitalWrite(ADDR0, HIGH);
      digitalWrite(ADDR1, LOW);
      break;
    case 4:  // Fourth segment (bottom)
      digitalWrite(ADDR0, LOW);
      digitalWrite(ADDR1, LOW);
      break;
  }
}

byte asciiToDL3416(char c) {
  // Convert character to display-compatible byte
  return (c >= ' ' && c <= '_') ? c : 0x20;
}

void setDataPins(byte data) {
  // Use shared data pins for setting data
  for (int i = 0; i < 7; i++) {
    digitalWrite(dataPins[i], (data >> i) & 0x01);
  }
}

void displayChar(char c, int wrPin) {
  // Display single character
  byte data = asciiToDL3416(c);
  setDataPins(data);
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

void updateTime() {
  // Get the current time directly from RTC each time
  text = rtc.getFormattedTime() + " " + rtc.getFormattedDate() + "  ";
  
  // Debug print to show time is updating
  Serial.println("Current Time: " + text);
}