#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <iterator>
#include "firmware_data.h"

// Hardware Definitions
const int PIN_MCLR = D10;
const int PIN_ICSPDAT = D0;
const int PIN_ICSPCLK = D1;

// Microchip ICSP Timing Specs (PIC16F17114)
const uint32_t T_ENTH_US = 500;
const uint32_t T_DLY_US = 100;
const uint32_t T_ERAB_MS = 15;
const uint32_t T_PINT_MS = 10;
const uint32_t T_CLOCK_US = 5;

// --- Low-Level Bit-Bang Helpers ---

// Explicit delays to satisfy PIC timing parameter constraints.
inline void tickDelay() {
    delayMicroseconds(T_CLOCK_US);
}

void bitBangWrite(uint32_t data, int num_bits) {
    // Set as output to drive the PIC
    pinMode(PIN_ICSPDAT, OUTPUT);

    // PIC Expects MSb First for command and payload data fields
    for (int i = num_bits - 1; i >= 0; i--) {
        digitalWrite(PIN_ICSPDAT, (data >> i) & 0x01);

        // Data changes on rising edge, latched on falling edge.
        // Verified timing: PIC samples strictly during stable HIGH period.
        tickDelay();
        digitalWrite(PIN_ICSPCLK, HIGH);
        tickDelay();
        digitalWrite(PIN_ICSPCLK, LOW);
    }

    // Ensure line returns to safe state
    digitalWrite(PIN_ICSPDAT, LOW);
}

uint32_t bitBangRead(int num_bits) {
    uint32_t val = 0;

    // Set to high-impedance input to let the PIC drive
    pinMode(PIN_ICSPDAT, INPUT);

    // Extra explicit turnaround delay to ensure bus yields
    delayMicroseconds(5);

    for (int i = num_bits - 1; i >= 0; i--) {
        tickDelay();
        digitalWrite(PIN_ICSPCLK, HIGH);

        // Standard consensus: Sample strictly inside the HIGH clock phase.
        // We delay slightly into the High phase to sample at dead-center stability.
        tickDelay();

        if (digitalRead(PIN_ICSPDAT)) {
            val |= (1UL << i);
        }

        digitalWrite(PIN_ICSPCLK, LOW);
    }

    // Reclaim the line for subsequent commands
    pinMode(PIN_ICSPDAT, OUTPUT);
    digitalWrite(PIN_ICSPDAT, LOW);

    return val;
}

// --- Core ICSP Primitives ---

void sendCommand(uint8_t cmd) {
    bitBangWrite(cmd, 8);
    delayMicroseconds(T_DLY_US);
}

void sendPayload(uint16_t data) {
    // 24-bit frame: [Start=0 (bit 23)] [Pad=000000 (bits 22-17)] [Data=16 bits (bits 16-1)] [Stop=0 (bit 0)]
    // The data is explicitly 16 bits, left-shifted by 1 to make space for the stop bit.
    // This construction preserves Bit 15 of the address (e.g. 0x8000 range), which previous code truncated.
    uint32_t frame = (static_cast<uint32_t>(data) & 0xFFFF) << 1;

    bitBangWrite(frame, 24);
    delayMicroseconds(T_DLY_US);
}

uint16_t readPayload() {
    // Read 24-bit frame from the device
    uint32_t frame = bitBangRead(24);

    // Extract the 16 bits contained in frame bits 16 through 1.
    // The 14-bit payload is safely within these 16 bits.
    uint16_t data = (frame >> 1) & 0xFFFF;

    delayMicroseconds(T_DLY_US);
    return data;
}

// --- Programming State Machine ---

bool enterLvp() {
    digitalWrite(PIN_ICSPCLK, LOW);
    digitalWrite(PIN_ICSPDAT, LOW);
    pinMode(PIN_ICSPCLK, OUTPUT);
    pinMode(PIN_ICSPDAT, OUTPUT);

    // Ensure MCLR holds PIC in reset
    digitalWrite(PIN_MCLR, LOW);
    delayMicroseconds(T_ENTH_US);

    // Shift in 32-bit LVP Key: 0x4D434850 strictly MSb First
    bitBangWrite(0x4D, 8);
    bitBangWrite(0x43, 8);
    bitBangWrite(0x48, 8);
    bitBangWrite(0x50, 8);

    delayMicroseconds(T_DLY_US);
    return true;
}

void exitLvp() {
    digitalWrite(PIN_MCLR, HIGH);
}

void bulkErase() {
    sendCommand(0x80);      // Load PC Address
    sendPayload(0x0000);    // Point to Flash

    sendCommand(0x18);      // Bulk Erase
    sendPayload(0x000A);    // Bit 1 = Flash, Bit 3 = Config

    delay(T_ERAB_MS);
}

void writeMemory(uint16_t start_addr, const uint16_t* data, size_t count, uint32_t row_size) {
    sendCommand(0x80);
    sendPayload(start_addr);

    for (size_t i = 0; i < count; i++) {
        bool end_of_row = (i + 1) % row_size == 0;
        sendCommand(end_of_row ? 0x00 : 0x02);
        sendPayload(data[i]);

        if (end_of_row) {
            sendCommand(0xE0); // Begin Internally Timed Programming
            delay(T_PINT_MS);

            if (i < count - 1) {
                sendCommand(0xF8); // Increment Address
            }
        }
    }
}

void verifyMemory(uint16_t start_addr, const uint16_t* data, size_t count) {
    sendCommand(0x80);
    sendPayload(start_addr);

    for (size_t i = 0; i < count; i++) {
        sendCommand(0xFE);  // Read Data and Increment PC
        uint16_t read_val = readPayload();
        uint16_t current_addr = start_addr + i;

        if (read_val != data[i]) {
            Serial.printf("Verify Failed at 0x%04X: Expected 0x%04X, Got 0x%04X\n", current_addr, data[i], read_val);
            return;
        }
    }
    Serial.printf("Verification Successful!\n");
}

// --- Arduino Framework ---

void setup() {
    Serial.begin(115200);
    while(!Serial);

    pinMode(PIN_MCLR, OUTPUT);
    digitalWrite(PIN_MCLR, HIGH); // Hold PIC in normal run mode

    pinMode(PIN_ICSPCLK, OUTPUT);
    digitalWrite(PIN_ICSPCLK, LOW);
    pinMode(PIN_ICSPDAT, OUTPUT);
    digitalWrite(PIN_ICSPDAT, LOW);

    Serial.println("Starting PIC LVP Programming Sequence...");

    enterLvp();

    // Check connection by reading Device ID (at PC 0x8006)
    sendCommand(0x80);
    sendPayload(0x8006);
    sendCommand(0xFC);
    uint16_t dev_id = readPayload();

    if (dev_id != 0x30DB) {
        Serial.printf("Error: Invalid Device ID: 0x%04X\n", dev_id);
        exitLvp();
        return;
    }

    Serial.println("Erasing...");
    bulkErase();

    Serial.println("Writing Flash...");
    writeMemory(0x0000, kProgramData, std::size(kProgramData), 32);

    Serial.println("Writing Config...");
    writeMemory(0x8007, kConfigData, std::size(kConfigData), 1);

    Serial.println("Verifying Flash...");
    verifyMemory(0x0000, kProgramData, std::size(kProgramData));

    Serial.println("Verifying Config...");
    verifyMemory(0x8007, kConfigData, std::size(kConfigData));

    exitLvp();
    Serial.println("Done.");
}

void loop() {
    // Programmer is idle
}
