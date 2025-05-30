#include <PCF8563Clock.h>
#include <EEPROM.h>

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

  // Set the initial time once
  if (EEPROM.read(0) == 1) {  // Check for our marker value
    rtc.setTime(
      0,   // second
      0,   // minute
      14,  // hour (24-hour format) - 14:00 = 2:00 PM
      30,  // day of month
      5,   // day of week (Friday)
      5,   // month (May)
      25   // year (2025 -> 25)
    );
    EEPROM.write(0, 0xFF);  // Write marker to indicate time has been set
  }

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
  // Add watchdog-style timing
  static unsigned long lastLoopTime = 0;
  unsigned long currentTime = millis();
  
  // Prevent loop from running too frequently
  if (currentTime - lastLoopTime < 50) {  // Minimum 50ms between updates
    return;
  }
  lastLoopTime = currentTime;

  // Format and update the time for display
  updateDisplayText();

  // Split text across displays
  splitTextForDisplays();

  // Display the text on all screens
  updateAllDisplays();

  // Add a small delay but don't block too long
  delay(50);
}

void updateDisplayText() {
  // Add I2C error handling and recovery
  static unsigned long lastUpdateTime = 0;
  static String lastValidTime = "14:00:00 2025/05/30";
  
  unsigned long currentMillis = millis();
  
  // Try to get time from RTC with error handling
  byte second, minute, hour, dayOfMonth, dayOfWeek, month, year;
  
  // Check if I2C is responding
  Wire.beginTransmission(0x51);  // PCF8563 default address
  byte error = Wire.endTransmission();
  
  if (error == 0) {  // I2C communication successful
    rtc.getTime(second, minute, hour, dayOfMonth, dayOfWeek, month, year);
    
    // Validate the time values
    if (hour <= 23 && minute <= 59 && second <= 59 && 
        month >= 1 && month <= 12 && dayOfMonth >= 1 && dayOfMonth <= 31) {
      
      // Format time and date
      char timeBuffer[9];
      char dateBuffer[11];
      snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d", hour, minute, second);
      snprintf(dateBuffer, sizeof(dateBuffer), "20%02d/%02d/%02d", year, month, dayOfMonth);
      
      displayText = String(timeBuffer) + " " + String(dateBuffer);
      lastValidTime = displayText;
      lastUpdateTime = currentMillis;
      
      Serial.println("RTC OK: " + displayText);
    } else {
      Serial.println("Invalid RTC data detected");
      displayText = lastValidTime;
    }
  } else {
    // I2C communication failed
    Serial.print("I2C Error: ");
    Serial.println(error);
    
    // Use last known good time or fall back to system millis
    if (currentMillis - lastUpdateTime > 5000) {  // If no update for 5 seconds
      Serial.println("Using fallback time display");
      displayText = "I2C ERROR       ";
    } else {
      displayText = lastValidTime;
    }
    
    // Try to reinitialize I2C
    Wire.end();
    delay(10);
    Wire.begin();
    rtc.begin();
  }
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