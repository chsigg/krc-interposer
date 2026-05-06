#include <Arduino.h>
#include <SPI.h>
#include "firmware_data.h"

// Hardware Definitions
const int PIN_MCLR = D10;
const int PIN_ICSPDAT = D0;
const int PIN_ICSPCLK = D1;
const int PIN_MISO_UNUSED = D9; // Unused by ICSP but required by SPI driver

// Internal pointer to the nRF SPIM register
NRF_SPIM_Type* spim_reg = NRF_SPIM0;

// Custom SPI instance for ICSP (PIC RA0/ICSPDAT <> XIAO D0, PIC RA1/ICSPCLK <> XIAO D1)
SPIClass icspSPI(spim_reg, PIN_MISO_UNUSED, PIN_ICSPCLK, PIN_ICSPDAT);

// Microchip ICSP Timing Specs (PIC16F17114)
const uint32_t T_ENTH_US = 250;
const uint32_t T_DLY_US = 1;
const uint32_t T_ERAB_MS = 9;  // 8.4ms max, rounded up
const uint32_t T_PINT_MS = 3;  // 2.8ms max, rounded up

// SPI settings (1 MHz, MSB first, Mode 1)
SPISettings kIcspSettings(1000000, MSBFIRST, SPI_MODE1);

// --- Core ICSP Primitives ---

void sendCommand(uint8_t cmd) {
    icspSPI.transfer(cmd);
    delayMicroseconds(T_DLY_US);
}

void sendPayload(uint16_t data) {
    // 24-bit payload: [Start=0] [Pad=0x00] [Data 13:0] [Stop=0]
    // Data is shifted left by 1 to make room for the Stop bit at bit 0.
    uint8_t b1 = 0x00;
    uint8_t b2 = (data >> 7) & 0xFF;
    uint8_t b3 = (data << 1) & 0xFE;

    icspSPI.transfer(b1);
    icspSPI.transfer(b2);
    icspSPI.transfer(b3);
    delayMicroseconds(T_DLY_US);
}

uint16_t readPayload() {
    // 1. Save the current MOSI pin mapping
    uint32_t mosi_pin = spim_reg->PSEL.MOSI;

    // 2. Disconnect MOSI so the PIC can drive the ICSPDAT line
    spim_reg->PSEL.MOSI = 0xFFFFFFFF;

    // 3. Clock in 24 bits
    [[maybe_unused]]
    uint8_t b1 = icspSPI.transfer(0x00);
    uint8_t b2 = icspSPI.transfer(0x00);
    uint8_t b3 = icspSPI.transfer(0x00);

    // 4. Reconnect MOSI
    spim_reg->PSEL.MOSI = mosi_pin;

    // 5. Extract the 14-bit data (Discard start, stop, and pad bits)
    uint16_t data = ((b2 & 0x7F) << 7) | (b3 >> 1);

    delayMicroseconds(T_DLY_US);
    return data;
}

// --- Programming State Machine ---

bool enterLvp() {
    digitalWrite(PIN_MCLR, LOW);
    delayMicroseconds(T_ENTH_US);

    icspSPI.beginTransaction(kIcspSettings);

    // Shift in LVP Key: 0x4D434850
    icspSPI.transfer(0x4D);
    icspSPI.transfer(0x43);
    icspSPI.transfer(0x48);
    icspSPI.transfer(0x50);

    delayMicroseconds(T_DLY_US);
    return true; // We assume success; verify will prove it
}

void exitLvp() {
    icspSPI.endTransaction();
    digitalWrite(PIN_MCLR, HIGH);
}

void bulkErase() {
    sendCommand(0x80);      // Load PC Address
    sendPayload(0x0000);    // Point to Flash

    sendCommand(0x18);      // Bulk Erase
    // Payload determines what to erase: Bit 1 = Flash, Bit 3 = Config
    sendPayload(0x000A);

    delay(T_ERAB_MS);        // Wait for erase to complete
}

void writeFlash() {
    sendCommand(0x80);      // Load PC Address
    sendPayload(0x0000);    // Start at 0x0000

    for (uint32_t i = 0; i < firmware_words; i++) {
        // Load data and increment PC. Use 0x00 for the last word of the row.
        uint8_t cmd = ((i + 1) % 32 == 0) ? 0x00 : 0x02;

        sendCommand(cmd);
        sendPayload(firmware_data[i]);

        // If we hit the 32nd word, burn the row
        if ((i + 1) % 32 == 0) {
            sendCommand(0xE0); // Begin Internally Timed Programming
            delay(T_PINT_MS);

            // Increment PC manually to next row if not at the very end
            if (i < firmware_words - 1) {
                sendCommand(0xF8); // Increment Address
            }
        }
    }
}

void verifyFlash() {
    sendCommand(0x80);      // Load PC Address
    sendPayload(0x0000);    // Start at 0x0000

    for (uint32_t i = 0; i < firmware_words; i++) {
        sendCommand(0xFE);  // Read Data and Increment PC
        uint16_t data = readPayload();

        if (data != firmware_data[i]) {
            Serial.printf("Verify Failed at 0x%04X: Expected 0x%04X, Got 0x%04X\n", i, firmware_data[i], data);
            return;
        }
    }
    Serial.println("Verification Successful!");
}

// --- Arduino Framework ---

void setup() {
    Serial.begin(115200);
    while(!Serial);

    pinMode(PIN_MCLR, OUTPUT);
    digitalWrite(PIN_MCLR, HIGH); // Hold PIC in normal run mode

    icspSPI.begin();

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
    writeFlash();

    Serial.println("Verifying...");
    verifyFlash();

    exitLvp();
}

void loop() {
    // Programmer is idle
}
