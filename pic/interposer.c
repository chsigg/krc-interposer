/*
 * Smart pass-through and override controller for an analog stove dial.
 * Reads physical 10k potentiometer, buffers to analog output, and
 * communicates with a 3.3V master device via 9600 baud UART.
 *
 * Microcontroller: PIC16F17114, Compiler: XC8
 */

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

// ==========================================
// Hardware Configuration Bits
// ==========================================

// Clock Source: Internal Oscillator
#pragma config FEXTOSC = OFF          // Disable external oscillator
#pragma config RSTOSC = HFINTOSC_1MHZ // Use internal 1MHz oscillator
#pragma config CLKOUTEN = OFF         // Disable clock out

// Watchdog Timer: Enabled, ~256ms timeout
#pragma config WDTE = ON         // Enable watchdog timer
#pragma config WDTCPS = WDTCPS_5 // Set WDT divider ratio to 1:1024 (approx 256ms)
#pragma config WDTCWS = WDTCWS_7 // Set WDT window to always open (100%)

// Power-up and Code Protect
#pragma config PWRTE = OFF      // Disable power-up timer
#pragma config MCLRE = ON       // MCLR pin is master clear
#pragma config CP = OFF         // Disable code memory protection
#pragma config LVP = ON         // Enable low voltage programming
#define _XTAL_FREQ 2000000      // 2 MHz CPU Clock

// Constants
#define SOFTWARE_WDT_MAX_TICKS 200  // 2 seconds / 10ms loop
#define PASSTHROUGH_BYTE       0x00
#define OFF_THRESHOLD          5
#define MIN_OVERRIDE_VAL       25
#define WAKEUP_THRESHOLD       230

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// ==========================================
// Global Variables
// ==========================================
volatile uint8_t rx_byte = PASSTHROUGH_BYTE;
volatile bool rx_received = false;
volatile uint16_t sw_watchdog = 0; // Counts 10ms ticks (200 ticks = 2 seconds)

// ==========================================
// Helper Functions
// ==========================================
uint8_t calculate_parity(uint8_t val) {
    val ^= val >> 4;
    val ^= val >> 2;
    val ^= val >> 1;
    return val & 1;
}

// ==========================================
// Interrupt Service Routine
// ==========================================
void __interrupt() ISR(void) {
    // UART RX Interrupt
    if (PIE3bits.RC1IE && PIR3bits.RC1IF) {
        uint8_t p_rx = RC1STAbits.RX9D;
        rx_byte = RC1REG; // Always read to clear flag

        uint8_t p_expected = calculate_parity(rx_byte);

        if (p_rx == p_expected) {
            sw_watchdog = 0;    // Reset SW WD on valid byte
            rx_received = true;
        }
    }

    // Timer1 Interrupt (Wake from sleep)
    if (PIE1bits.TMR1IE && PIR1bits.TMR1IF) {
        PIR1bits.TMR1IF = 0; // Clear flag
        TMR1 = 65226; // Reload for 10ms (65536 - 310)
    }
}

// ==========================================
// Hardware Initialization
// ==========================================
void init_hardware(void) {
    // ------------------------------------------
    // 1. Clock Configuration
    // ------------------------------------------

    // Configure HFINTOSC to 2 MHz.
    OSCFRQ = 0x01;

    // ------------------------------------------
    // 2. I/O Pin Configuration
    // ------------------------------------------

    // RA4 (Analog In): wiper of physical dial
    TRISAbits.TRISA4 = 1;   // Input
    ANSELAbits.ANSELA4 = 1; // Analog

    // RA5 (Dial Low-Side Switch): Acts as ground path
    TRISAbits.TRISA5 = 1;   // High-Z
    ANSELAbits.ANSELA5 = 0; // Digital
    LATAbits.LATA5 = 0;     // Pre-set latch to 0 for when driven low

    // RA2 (Analog Out): Internal Op-Amp output
    TRISAbits.TRISA2 = 0;   // Output
    ANSELAbits.ANSELA2 = 1; // Analog

    // UART TX Pin (RA0)
    WPUAbits.WPUA0 = 0;     // Disable Weak Pull-Up
    LATAbits.LATA0 = 0;     // Clear pin latch
    ODCAbits.ODCA0 = 1;     // Open-Drain
    TRISAbits.TRISA0 = 0;   // Output

    // UART RX Pin (RA1)
    TRISAbits.TRISA1 = 1;   // Input
    ANSELAbits.ANSELA1 = 0; // Digital

    // Peripheral Pin Select (PPS)
    RX1PPS = 0x01;          // Route RX to RA1
    RA0PPS = 0x14;          // Route TX1 to RA0

    // ------------------------------------------
    // 3. Peripheral Configuration
    // ------------------------------------------

    // ADC (ADCC): 12-bit ADC
    ADCON0bits.ON = 1;      // Enable ADC
    ADCON0bits.CS = 1;      // ADCRC clock source
    ADCON0bits.FM = 1;      // Right justified result
    // Select channel RA4 (ADPCH register usage depends on exact device)
    ADPCH = 0x04;           // Assuming channel 4 is RA4

    // DAC: 8-bit DAC
    DAC1CON0bits.EN = 1;    // Enable DAC

    // Op-Amp (OPA1): Unity-gain buffer
    // Unity gain buffer. Input from DAC, Output to RA2
    OPA1CON1bits.NCH = 0b001; // Negative input connects to OPA output (Unity)
    OPA1CON1bits.PCH = 0b010; // Positive input connects to DAC1
    OPA1CON0bits.EN  = 0;     // Initially disabled (Safety cutoff active)

    // UART: 9600 baud using 16-bit BRG
    // Baud = Fosc / (4 * (SPBRG + 1)) when BRG16=1 and BRGH=1
    // SPBRG = (2000000 / (4 * 9600)) - 1 = 51.08 -> 51
    BAUD1CONbits.BRG16 = 1;
    TX1STAbits.BRGH = 1;
    SP1BRGL = 51;
    SP1BRGH = 0;

    // Configure for 9-bit mode (used for software parity)
    TX1STAbits.TX9 = 1;  // Enable 9-bit transmission
    RC1STAbits.RX9 = 1;  // Enable 9-bit reception

    TX1STAbits.TXEN = 1;    // Enable transmitter
    RC1STAbits.CREN = 1;    // Enable continuous receive
    RC1STAbits.SPEN = 1;    // Enable serial port

    // ------------------------------------------
    // 4. Interrupt Configuration
    // ------------------------------------------
    PIE3bits.RC1IE = 1;     // Enable UART RX interrupt
    INTCONbits.PEIE = 1;    // Enable peripheral interrupts
    INTCONbits.GIE = 1;     // Enable global interrupts

    // ------------------------------------------
    // 5. Timer1 Configuration (LFINTOSC for Sleep)
    // ------------------------------------------
    OSCENbits.LFIOREN = 1;  // Enable LFINTOSC
    T1CLK = 0x03;           // Select LFINTOSC as clock source
    T1CONbits.CKPS = 0b00;  // 1:1 Prescaler
    TMR1 = 65226;           // Load for 10ms overflow
    PIE1bits.TMR1IE = 1;    // Enable Timer1 interrupt
    T1CONbits.ON = 1;       // Start Timer1
}

// ==========================================
// ADC Measurement Function
// ==========================================
uint8_t read_adc(void) {
    // 1. Configure RA5 as output and drive it LOW
    TRISAbits.TRISA5 = 0;

    // 2. Wait brief settling period (50us)
    __delay_us(50);

    // 3. Trigger ADC conversion
    ADCON0bits.GO = 1;
    while (ADCON0bits.GO); // Wait for completion

    // Scale 12-bit result to 8-bit (0-255)
    uint8_t scaled_adc = (uint8_t)(ADRES >> 4);

    // 4. Configure RA5 as Input (High-Z) to cut power
    TRISAbits.TRISA5 = 1;

    return scaled_adc;
}

// ==========================================
// Main Program
// ==========================================
int main(void) {
    init_hardware();

    uint8_t output_val = 0;

    while(1) {
        CLRWDT(); // Clear Hardware Watchdog Timer

        if (sw_watchdog < SOFTWARE_WDT_MAX_TICKS) {
            ++sw_watchdog;
        }

        uint8_t adc_value = read_adc();

        if (rx_received || adc_value > WAKEUP_THRESHOLD) {
            rx_received = false;
            while (!PIR3bits.TX1IF);
            TX1STAbits.TX9D = calculate_parity(adc_value);
            TX1REG = adc_value; // Send ADC value
        }

        if (adc_value < OFF_THRESHOLD) {
            rx_byte = PASSTHROUGH_BYTE;
        }

        if (rx_byte == PASSTHROUGH_BYTE) {
            output_val = adc_value;
        } else if (sw_watchdog >= SOFTWARE_WDT_MAX_TICKS) {
            output_val = OFF_THRESHOLD;
        } else {
            uint8_t target_val = MAX(rx_byte, MIN_OVERRIDE_VAL);
            if (target_val > output_val) {
                    output_val += MIN(target_val - output_val, 3);
            } else if (target_val < output_val) {
                    output_val -= MIN(output_val - target_val, 3);
            }
        }

        DAC1CON1 = output_val;
        OPA1CON0bits.EN = output_val >= OFF_THRESHOLD;

        SLEEP();  // Sleep until Timer1 or UART RX interrupt
    }

    return 0;
}

