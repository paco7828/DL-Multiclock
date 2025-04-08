#include <PCF8563Clock.h>

const byte N_OF_SEGMENTS = 4;  // Number of segments per display
const byte SCREEN_COUNT = 5;   // Number of displays

// Shared pins
const byte CLR = A0;
const byte ADDR0 = A1;
const byte ADDR1 = A2;
const byte dataPins[7] = { 7, 8, 9, 10, 11, 12, 13 };

// Write pins for each display
const byte writePins[SCREEN_COUNT] = { 2, 3, 4, 5, 6 };

String displayText = "";            // Text to be displayed on screens
String splittedText[SCREEN_COUNT];  // Text split for each display

PCF8563Clock rtc;

void setup() {
  Serial.begin(9600);
  rtc.begin();

  // Set the initial time
  rtc.setTime(0, 56, 23, 28, 5, 3, 25);  // 23:56:00 on March 28, 2025

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
  // Format and update the time for display
  updateDisplayText();

  // Split text across displays
  splitTextForDisplays();

  // Display the text on all screens
  updateAllDisplays();

  // Small delay to avoid flickering and reduce CPU usage
  delay(100);
}

void updateDisplayText() {
  // Format time and date to fit across 5 screens (20 segments)
  // Format: "HH:MM:SS DD/MM/YY"
  String timeStr = rtc.getHour() + ":" + rtc.getMinute() + ":" + rtc.getSecond();
  String dateStr = rtc.getYear() + "/" + rtc.getMonth() + "/" + rtc.getDayNum();
  displayText = timeStr + " " + dateStr;

  // Debug print to show time is updating
  Serial.println("Current Time: " + displayText);
}

void splitTextForDisplays() {
  // Clear previous text segments
  for (int i = 0; i < SCREEN_COUNT; i++) {
    splittedText[i] = "    ";  // Initialize with spaces
  }

  // Split text safely
  int textLength = min((int)displayText.length(), N_OF_SEGMENTS * SCREEN_COUNT);

  for (int i = 0; i < textLength; i++) {
    int screenIndex = i / N_OF_SEGMENTS;
    int segmentIndex = i % N_OF_SEGMENTS;

    if (screenIndex < SCREEN_COUNT) {
      // Replace character at specific position
      splittedText[screenIndex].setCharAt(segmentIndex, displayText.charAt(i));
    }
  }
}

void updateAllDisplays() {
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