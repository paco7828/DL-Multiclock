#include "dl3416.h"

#define N_OF_SEGMENTS 4
#define SCREEN_COUNT 5

// Shared pins
const byte CLR = A3;
const byte dataPins[7] = { 7, 8, 9, 10, 11, 12, 13 };

// Shift register control pins
const byte SHIFT = A0;
const byte LATCH = A1;
const byte SERIN = A2;

// Write pins for each display
const byte writePins[SCREEN_COUNT] = {2, 3, 4, 5, 6};

// Create display objects
DL3416 displays[SCREEN_COUNT] = {
  DL3416(0, 0, writePins[0], dataPins),
  DL3416(1, 0, writePins[1], dataPins),
  DL3416(0, 1, writePins[2], dataPins),
  DL3416(1, 1, writePins[3], dataPins),
  DL3416(0, 0, writePins[4], dataPins)
};

const String text = "ABCDEFGHIJKLMNOPQRST";
String splittedText[SCREEN_COUNT];

void clearDisplays() {
  digitalWrite(CLR, LOW);
  delayMicroseconds(100);
  digitalWrite(CLR, HIGH);
  delayMicroseconds(100);
}

void shiftOutTwoRegisters(byte a0Data, byte a1Data) {
  // Ensure only Q0-Q4 are used, Q5-Q7 are always 0
  a0Data &= 0x1F;  // Clear upper 3 bits
  a1Data &= 0x1F;  // Clear upper 3 bits

  digitalWrite(LATCH, LOW);
  
  // First shift out A1 data (second shift register)
  shiftOut(SERIN, SHIFT, MSBFIRST, a1Data);
  
  // Then shift out A0 data (first shift register)
  shiftOut(SERIN, SHIFT, MSBFIRST, a0Data);
  
  digitalWrite(LATCH, HIGH);
}

void setup() {
  Serial.begin(9600);

  // Initialize Clear pin
  pinMode(CLR, OUTPUT);
  digitalWrite(CLR, HIGH);

  // Initialize shift register pins
  pinMode(SHIFT, OUTPUT);
  pinMode(LATCH, OUTPUT);
  pinMode(SERIN, OUTPUT);

  // Initialize shared data pins
  for (int i = 0; i < 7; i++) {
    pinMode(dataPins[i], OUTPUT);
  }

  // Clear displays
  clearDisplays();

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

  // Initialize displays
  for (int i = 0; i < SCREEN_COUNT; i++) {
    displays[i].begin();
  }
}

void loop() {
  for (int i = 0; i < SCREEN_COUNT; i++) {
    // Segment addressing sequence for each segment
    byte segmentAddresses[4][2] = {
      {0b00000001, 0b00000001},  // LOW-LOW (first segment)
      {0b00000001, 0b00000010},  // LOW-HIGH (second segment)
      {0b00000010, 0b00000001},  // HIGH-LOW (third segment)
      {0b00000010, 0b00000010}   // HIGH-HIGH (fourth segment)
    };

    // Select display
    shiftOutTwoRegisters(1 << i, 1 << i);

    // Display characters on each segment
    for (int j = 0; j < N_OF_SEGMENTS; j++) {
      // Set segment address
      shiftOutTwoRegisters(segmentAddresses[j][0], segmentAddresses[j][1]);

      // Display character
      displays[i].displayChar(splittedText[i][j]);
    }
  }

  delay(1000);  // Update every second
}