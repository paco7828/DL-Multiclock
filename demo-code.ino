// Shift Register Control Pins
#define DATA_PIN   PC0  // DS (Serial Data) - 74HC595
#define CLOCK_PIN  PC1  // SH_CP (Shift Clock) - 74HC595
#define LATCH_PIN  PC2  // ST_CP (Latch Clock) - 74HC595

// DL3416 Control Pins
#define WR_PIN     2   // Write (Active Low)
#define CLR_PIN    4   // Clear (Active Low)
#define BL_PIN     3   // Backlight (Can be PWM for dimming)

// Parallel Data Bus (D0-D6)
const int dataBus[7] = {5, 6, 7, 8, 9, 10, 11};  // DL3416 Data Pins

// Display Parameters
#define NUM_DISPLAYS 6
#define CHARS_PER_DISPLAY 4
#define TOTAL_CHARS (NUM_DISPLAYS * CHARS_PER_DISPLAY)

// Sample text to display
char message[TOTAL_CHARS + 1] = "HELLO, THIS IS A TEST MSG!  "; 

// Function to send a single character to a specific display position
void writeCharToDisplay(int pos, char c) {
    if (pos < 0 || pos >= TOTAL_CHARS) return;  // Out of range

    // Convert character to 7-bit ASCII (Only first 128 chars are supported)
    byte charData = (byte)c & 0x7F;

    // Determine which display and which character within the display
    int displayNum = pos / CHARS_PER_DISPLAY;  // 0-5
    int charPos = pos % CHARS_PER_DISPLAY;     // 0-3

    // Compute A0 and A1 based on character position
    byte a0 = (charPos & 0x01) ? HIGH : LOW;
    byte a1 = (charPos & 0x02) ? HIGH : LOW;

    // Send A0/A1 values using shift registers
    shiftOutData(displayNum, a0, a1);

    // Send character data to DL3416
    for (int i = 0; i < 7; i++) {
        digitalWrite(dataBus[i], (charData >> i) & 1);
    }

    // Write pulse (Active Low)
    digitalWrite(WR_PIN, LOW);
    delayMicroseconds(1);
    digitalWrite(WR_PIN, HIGH);
}

// Function to send data to shift registers for A0/A1 control
void shiftOutData(int displayNum, byte a0, byte a1) {
    byte shiftRegisterData = 0;
    
    // Set A0 values in the first shift register
    if (a0) shiftRegisterData |= (1 << displayNum);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, shiftRegisterData);

    // Set A1 values in the second shift register
    shiftRegisterData = 0;
    if (a1) shiftRegisterData |= (1 << displayNum);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, shiftRegisterData);

    // Latch the data
    digitalWrite(LATCH_PIN, LOW);
    delayMicroseconds(1);
    digitalWrite(LATCH_PIN, HIGH);
}

// Function to update the entire display with a message
void updateDisplay() {
    for (int i = 0; i < TOTAL_CHARS; i++) {
        writeCharToDisplay(i, message[i]);
    }
}

void setup() {
    pinMode(DATA_PIN, OUTPUT);
    pinMode(CLOCK_PIN, OUTPUT);
    pinMode(LATCH_PIN, OUTPUT);
    pinMode(WR_PIN, OUTPUT);
    pinMode(CLR_PIN, OUTPUT);
    pinMode(BL_PIN, OUTPUT);

    // Set data bus pins as output
    for (int i = 0; i < 7; i++) {
        pinMode(dataBus[i], OUTPUT);
    }

    // Initialize Display
    digitalWrite(CLR_PIN, HIGH);  // No clear
    digitalWrite(WR_PIN, HIGH);   // No write
    digitalWrite(BL_PIN, HIGH);   // Enable backlight
}

void loop() {
    updateDisplay();  // Update all 24 characters
    delay(1000);      // Update every second
}
