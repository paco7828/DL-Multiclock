#include "dl3416.h"

#define N_OF_SEGMENTS 4
#define SCREEN_COUNT 2

// Shared pins
const byte CLR = 2;
const byte dataPins[7] = { 9, 10, 11, 12, 13, A0, A1 };

// Address and WR pins for each display
DL3416 display1(3, 4, 5, dataPins);  // First display
DL3416 display2(6, 7, 8, dataPins);  // Second display

const String text = "NODE JS ";
String splittedText[SCREEN_COUNT];

void setup() {
  // Initialize Clear pin
  pinMode(CLR, OUTPUT);
  digitalWrite(CLR, HIGH);

  // Initialize shared data pins
  for (int i = 0; i < 7; i++) {
    pinMode(dataPins[i], OUTPUT);
  }

  // Initialize displays
  display1.begin();
  display2.begin();

  // Split text safely
  int count = 0;
  for (int i = 0; i < text.length() && count < SCREEN_COUNT; i += N_OF_SEGMENTS) {
    splittedText[count] = text.substring(i, min(i + N_OF_SEGMENTS, text.length()));
    count++;
  }
}

void loop() {
  // Display text on each screen
  display1.displayText(splittedText[0].c_str());
  display2.displayText(splittedText[1].c_str());
}