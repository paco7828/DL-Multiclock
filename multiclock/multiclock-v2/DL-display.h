class DLDisplay {
private:
  // Shift register pins
  byte SRCLK = 0;
  byte RCLK = 0;
  byte SER = 0;

  // All Displays' WR# pins
  const byte DISPLAY_WR_BITS[5] = { 3, 4, 5, 6, 7 };

  // Displayed text stored in buffer
  char displayBuffer[21] = "                   ";

  // Helper variables
  unsigned long lastRefresh = 0;
  byte currentDisplay = 0;
  byte currentDigit = 0;

  // Function for shift registers
  void shiftOut16(uint16_t data) {
    digitalWrite(this->RCLK, LOW);

    for (int i = 15; i >= 0; i--) {
      digitalWrite(this->SER, (data >> i) & 1);
      digitalWrite(this->SRCLK, HIGH);
      delayMicroseconds(1);
      digitalWrite(this->SRCLK, LOW);
      delayMicroseconds(1);
    }

    digitalWrite(this->RCLK, HIGH);
    delayMicroseconds(1);
  }

  // Helper function for displays
  byte asciiToDL(char c) {
    if (c >= ' ' && c <= '_') {
      return c;
    }
    return 0x20;  // Space character
  }

public:
  void begin(byte SRCLK, byte RCLK, byte SER) {
    // Store params as pins
    this->SRCLK = SRCLK;
    this->RCLK = RCLK;
    this->SER = SER;

    // Set pins as output
    pinMode(this->SRCLK, OUTPUT);
    pinMode(this->RCLK, OUTPUT);
    pinMode(this->SER, OUTPUT);

    // Clear display
    uint16_t initData = 0x00F9;
    this->shiftOut16(initData);
    delay(50);
    clearDisplay();
  }

  void clearDisplay() {
    // Clear all displays by pulsing CLR LOW
    uint16_t clearData = 0x00F8;  // CLR=0, all WR=1
    this->shiftOut16(clearData);
    delay(15);

    // Set CLR HIGH again
    clearData |= 0x01;  // CLR=1
    this->shiftOut16(clearData);

    // Clear buffer
    setDisplayText("                    ");  // 20 spaces
  }

  void refreshDisplay() {
    // Refresh at high frequency - 5 displays x 4 digits = 20 total positions
    // Refresh each position every 1ms for smooth display
    if (micros() - this->lastRefresh >= 1000) {  // 1ms per digit

      // Calculate which character we're displaying (0-19)
      int charIndex = this->currentDisplay * 4 + this->currentDigit;

      // Get the WR bit for current display
      int wrBit = DISPLAY_WR_BITS[this->currentDisplay];

      // Build control byte: CLR=1, A0/A1 for digit position, appropriate WR bit
      uint16_t controlBits = 0x01;  // CLR=1 (bit 0)

      // Set address bits for current digit (A0=bit1, A1=bit2)
      switch (this->currentDigit + 1) {
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
      byte data = this->asciiToDL(this->displayBuffer[charIndex]);

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
      this->shiftOut16(outputData);
      delayMicroseconds(2);

      // Pulse the appropriate WR line LOW to write data to current display
      outputData &= ~(1 << wrBit);  // Clear the WR bit for current display
      this->shiftOut16(outputData);
      delayMicroseconds(2);

      // Set WR back HIGH
      outputData |= (1 << wrBit);
      this->shiftOut16(outputData);

      // Move to next position
      this->currentDigit++;
      if (this->currentDigit >= 4) {
        this->currentDigit = 0;
        this->currentDisplay = (this->currentDisplay + 1) % 5;
      }
      this->lastRefresh = micros();
    }
  }

  void setDisplayText(const char* text) {
    // Copy text to display buffer (up to 20 characters)
    int textLen = strlen(text);

    for (int i = 0; i < 20; i++) {
      if (i < textLen) {
        this->displayBuffer[i] = text[i];
      } else {
        this->displayBuffer[i] = ' ';  // Fill remaining with spaces
      }
    }
    this->displayBuffer[20] = '\0';  // Null terminate
  }

  void fullTest() {
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
      while (millis() - startTime < 100) {
        refreshDisplay();
      }
    }
    clearDisplay();
  }
};