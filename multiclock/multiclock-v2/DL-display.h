// ESP32 C3 SuperMini with 2x 74HC595 controlling 5 displays (20 characters total)
// Pin definitions
#define SRCLK_PIN 3  // Shift register clock
#define RCLK_PIN 4   // Register clock (latch)
#define SER_PIN 5    // Serial data input

// WR bit positions for each display in first 74HC595
#define DISPLAY_1_WR_BIT 3  // QD
#define DISPLAY_2_WR_BIT 4  // QE
#define DISPLAY_3_WR_BIT 5  // QF
#define DISPLAY_4_WR_BIT 6  // QG
#define DISPLAY_5_WR_BIT 7  // QH

// Display buffer to hold current text (20 characters + null terminator)
char displayBuffer[21] = "                    ";  // 20 spaces
unsigned long lastRefresh = 0;
int currentDisplay = 0;  // Which display (0-4)
int currentDigit = 0;    // Which digit in current display (0-3)

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting 5-display system (20 characters)...");

  // Initialize pins
  pinMode(SRCLK_PIN, OUTPUT);
  pinMode(RCLK_PIN, OUTPUT);
  pinMode(SER_PIN, OUTPUT);

  // Initialize displays
  displayBegin();
  delay(100);
}

void loop() {
  // Continuously refresh displays (multiplexing)
  refreshDisplay();

  // Demo sequence
  static unsigned long lastChange = 0;

  if (millis() - lastChange > 3000) {
    setDisplayText("HELLO WORLD 12345678");
    lastChange = millis();
  }
}

void shiftOut16(uint16_t data) {
  digitalWrite(RCLK_PIN, LOW);

  for (int i = 15; i >= 0; i--) {
    digitalWrite(SER_PIN, (data >> i) & 1);
    digitalWrite(SRCLK_PIN, HIGH);
    delayMicroseconds(1);
    digitalWrite(SRCLK_PIN, LOW);
    delayMicroseconds(1);
  }

  digitalWrite(RCLK_PIN, HIGH);
  delayMicroseconds(1);
}

void displayBegin() {
  // Initialize with all WR lines HIGH, CLR HIGH
  uint16_t initData = 0x00F9;  // All WR bits + CLR bit high
  shiftOut16(initData);
  delay(50);

  // Clear all displays
  clearDisplay();
}

byte asciiToDL(char c) {
  if (c >= ' ' && c <= '_') {
    return c;
  }
  return 0x20;  // Space character
}

void refreshDisplay() {
  // Refresh at high frequency - 5 displays x 4 digits = 20 total positions
  // Refresh each position every 1ms for smooth display
  if (micros() - lastRefresh >= 1000) {  // 1ms per digit

    // Calculate which character we're displaying (0-19)
    int charIndex = currentDisplay * 4 + currentDigit;

    // Get the WR bit for current display
    int wrBit = DISPLAY_1_WR_BIT + currentDisplay;

    // Build control byte: CLR=1, A0/A1 for digit position, appropriate WR bit
    uint16_t controlBits = 0x01;  // CLR=1 (bit 0)

    // Set address bits for current digit (A0=bit1, A1=bit2)
    switch (currentDigit + 1) {
      case 1:
        controlBits |= 0x06;  // A0=1, A1=1
        break;
      case 2:
        controlBits |= 0x04;  // A0=0, A1=1
        break;
      case 3:
        controlBits |= 0x02;  // A0=1, A1=0
        break;
      case 4:
        // A0=0, A1=0 (no additional bits)
        break;
    }

    // Set all WR bits HIGH initially
    controlBits |= 0xF8;  // Set bits 3,4,5,6,7 (all WR lines HIGH)

    // Get character data for current position
    byte data = asciiToDL(displayBuffer[charIndex]);

    // Combine control and data (data goes to second shift register)
    uint16_t outputData = controlBits;

    // Set data bits D0-D6 in upper byte (second shift register)
    if (data & 0x01) outputData |= 0x0100;  // D0 -> bit 8
    if (data & 0x02) outputData |= 0x0200;  // D1 -> bit 9
    if (data & 0x04) outputData |= 0x0400;  // D2 -> bit 10
    if (data & 0x08) outputData |= 0x0800;  // D3 -> bit 11
    if (data & 0x10) outputData |= 0x1000;  // D4 -> bit 12
    if (data & 0x20) outputData |= 0x2000;  // D5 -> bit 13
    if (data & 0x40) outputData |= 0x4000;  // D6 -> bit 14

    // Output data with all WR HIGH
    shiftOut16(outputData);
    delayMicroseconds(2);

    // Pulse the appropriate WR line LOW to write data to current display
    outputData &= ~(1 << wrBit);  // Clear the WR bit for current display
    shiftOut16(outputData);
    delayMicroseconds(2);

    // Set WR back HIGH
    outputData |= (1 << wrBit);
    shiftOut16(outputData);

    // Move to next position
    currentDigit++;
    if (currentDigit >= 4) {
      currentDigit = 0;
      currentDisplay = (currentDisplay + 1) % 5;
    }

    lastRefresh = micros();
  }
}

void setDisplayText(const char* text) {
  // Copy text to display buffer (up to 20 characters)
  int textLen = strlen(text);

  for (int i = 0; i < 20; i++) {
    if (i < textLen) {
      displayBuffer[i] = text[i];
    } else {
      displayBuffer[i] = ' ';  // Fill remaining with spaces
    }
  }
  displayBuffer[20] = '\0';  // Null terminate

  Serial.print("Set display text: '");
  Serial.print(displayBuffer);
  Serial.println("'");
}

void clearDisplay() {
  // Clear all displays by pulsing CLR LOW
  uint16_t clearData = 0x00F8;  // CLR=0, all WR=1
  shiftOut16(clearData);
  delay(15);

  // Set CLR HIGH again
  clearData |= 0x01;  // CLR=1
  shiftOut16(clearData);

  // Clear buffer
  setDisplayText("                    ");  // 20 spaces
}

void scrollTextOnce(const char* message, int scrollSpeed) {
  int messageLen = strlen(message);
  int totalPositions = messageLen + 20;  // Message length + display width for complete scroll

  Serial.print("Scrolling: ");
  Serial.println(message);
  Serial.print("Message length: ");
  Serial.print(messageLen);
  Serial.print(", Total scroll positions: ");
  Serial.println(totalPositions);

  unsigned long lastScroll = millis();
  int scrollPos = 0;

  while (scrollPos < totalPositions) {
    // Keep refreshing displays during scroll
    refreshDisplay();

    // Check if it's time to advance scroll
    if (millis() - lastScroll >= scrollSpeed) {
      char tempBuffer[21];  // 20 chars + null terminator

      // Fill the 20-character window
      for (int i = 0; i < 20; i++) {
        int charIndex = scrollPos + i - 20;  // Start off-screen
        if (charIndex >= 0 && charIndex < messageLen) {
          tempBuffer[i] = message[charIndex];
        } else {
          tempBuffer[i] = ' ';  // Empty space
        }
      }
      tempBuffer[20] = '\0';

      setDisplayText(tempBuffer);
      scrollPos++;
      lastScroll = millis();
    }
  }

  // Show final message for a moment
  setDisplayText("SCROLL COMPLETE     ");
  unsigned long endTime = millis();
  while (millis() - endTime < 1000) {
    refreshDisplay();
  }
}

// Blocking version for simple use (not recommended during normal operation)
void displayTextBlocking(const char* text) {
  setDisplayText(text);

  // Refresh for 200ms to make it visible
  unsigned long startTime = millis();
  while (millis() - startTime < 200) {
    refreshDisplay();
  }
}

void fullTest() {
  Serial.println("Full test starting...");

  // Test each display individually
  for (int display = 0; display < 5; display++) {
    char testText[21] = "                    ";

    // Light up current display with pattern
    for (int digit = 0; digit < 4; digit++) {
      testText[display * 4 + digit] = '1' + display;
    }

    setDisplayText(testText);
    Serial.print("Testing display ");
    Serial.println(display + 1);

    // Refresh for 1 second
    unsigned long startTime = millis();
    while (millis() - startTime < 1000) {
      refreshDisplay();
    }
  }

  // Test all characters
  for (char c = ' '; c <= 'Z'; c++) {
    char testPattern[21];
    for (int i = 0; i < 20; i++) {
      testPattern[i] = c;
    }
    testPattern[20] = '\0';

    setDisplayText(testPattern);

    // Refresh for 300ms
    unsigned long startTime = millis();
    while (millis() - startTime < 300) {
      refreshDisplay();
    }
  }

  clearDisplay();
  Serial.println("Full test complete!");
}