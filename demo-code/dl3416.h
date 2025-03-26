#include <Arduino.h>

class DL3416 {
private:
  byte addr0, addr1, wr;
  const byte* sharedDataPins;  // Pointer to shared data pins

public:
  // Constructor with shared data pins reference
  DL3416(byte addr0, byte addr1, byte wr, const byte* dataPins)
    : addr0(addr0), addr1(addr1), wr(wr), sharedDataPins(dataPins) {
  }

  void begin() {
    // Configure address and write pins
    pinMode(addr0, OUTPUT);
    pinMode(addr1, OUTPUT);
    pinMode(wr, OUTPUT);
    digitalWrite(wr, HIGH);
  }

  void selectAddr(byte segment) {
    // Segment addressing logic remains the same
    switch (segment) {
      case 1:
        digitalWrite(addr0, HIGH);
        digitalWrite(addr1, HIGH);
        break;
      case 2:
        digitalWrite(addr0, LOW);
        digitalWrite(addr1, HIGH);
        break;
      case 3:
        digitalWrite(addr0, HIGH);
        digitalWrite(addr1, LOW);
        break;
      case 4:
        digitalWrite(addr0, LOW);
        digitalWrite(addr1, LOW);
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
      digitalWrite(sharedDataPins[i], (data >> i) & 0x01);
    }
  }

  void displayChar(char c) {
    // Display single character
    byte data = asciiToDL3416(c);
    setDataPins(data);
    digitalWrite(wr, LOW);
    delayMicroseconds(10);
    digitalWrite(wr, HIGH);
  }

  void displayText(const char* message) {
    // Display text across 4 segments
    for (int i = 0; i < 4 && message[i] != '\0'; i++) {
      selectAddr(i + 1);
      displayChar(message[i]);
    }
  }
};