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
#pragma config FEXTOSC = OFF    // External Oscillator Selection -> Oscillator not enabled
#pragma config RSTOSC = HFINTOSC_1MHZ // Reset Oscillator Selection -> HFINTOSC (1MHz)
#pragma config CLKOUTEN = OFF   // Clock Out Enable -> CLKOUT function is disabled
// Watchdog Timer: Enabled, ~256ms timeout
#pragma config WDTE = ON        // Watchdog Timer Enable -> WDT enabled
#pragma config WDTCPS = WDTCPS_5 // WDT Period Select -> Divider ratio 1:1024 (approx 256ms)
#pragma config WDTCWS = WDTCWS_7 // WDT Window Select -> Window always open (100%)
// Power-up and Code Protect
#pragma config PWRTE = OFF      // Power-up Timer Enable -> PWRT disabled
#pragma config MCLRE = ON       // Master Clear Enable -> MCLR pin is Master Clear
#pragma config CP = OFF         // Code Protection -> Program memory code protection disabled
#pragma config LVP = ON         // Low Voltage Programming -> Low voltage programming enabled
#define _XTAL_FREQ 2000000 // 2 MHz CPU Clock
// Constants
#define SOFTWARE_WDT_MAX_TICKS 200 // 2 seconds / 10ms loop
#define OVERRIDE_OFF_BYTE      0x00
#define SAFETY_CUTOFF_VAL      25
#define TELEMETRY_THRESHOLD    230

// ==========================================
// Global Variables
// ==========================================
volatile uint8_t rx_byte = 0x00;
volatile uint16_t sw_watchdog = 0; // Counts 10ms ticks (200 ticks = 2 seconds)
// State Flags
volatile bool is_telemetry_on = false;
volatile bool is_override_on = false;

// ==========================================
// Interrupt Service Routine
// ==========================================
void __interrupt() ISR(void) {
    // UART RX Interrupt
    if (!PIE3bits.RC1IE || !PIR3bits.RC1IF) return;
    
    rx_byte = RC1REG; // Always read to clear flag
    sw_watchdog = 0;    // Reset SW WD on valid byte
    
    // RX byte evaluation & Safety Interlock
    is_override_on = is_telemetry_on && (rx_byte != OVERRIDE_OFF_BYTE);
}

// ==========================================
// Hardware Initialization
// ==========================================
void init_hardware(void) {
    // ------------------------------------------
    // 1. Clock Configuration
    // ------------------------------------------
    // Configure HFINTOSC to 2 MHz.
    // On many PIC16s, OSCFRQ = 0x01 selects 2MHz.
    OSCFRQ = 0x01; 
    
    // ------------------------------------------
    // 2. I/O Pin Configuration
    // ------------------------------------------
    
    // RA4 (Analog In): wiper of physical dial
    TRISAbits.TRISA4 = 1;   // Input
    ANSELAbits.ANSELA4 = 1; // Analog
    
    // RA5 (Dial Low-Side Switch): Acts as ground path
    // Start as Input (High-Z) to save power
    TRISAbits.TRISA5 = 1;   // High-Z
    ANSELAbits.ANSELA5 = 0; // Digital
    LATAbits.LATA5 = 0;     // Pre-set latch to 0 for when driven low
    
    // RA2 (Analog Out): Internal Op-Amp output
    TRISAbits.TRISA2 = 0;   // Output
    ANSELAbits.ANSELA2 = 1; // Analog
    
    // UART TX Pin (RA0) STRICT Initialization Sequence
    // 1) Disable Weak Pull-Up
    WPUAbits.WPUA0 = 0;
    // 2) Clear pin latch
    LATAbits.LATA0 = 0;
    // 3) Enable Open-Drain
    ODCAbits.ODCA0 = 1;
    // 4) Configure as Output
    TRISAbits.TRISA0 = 0;
    
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
    
    TX1STAbits.TXEN = 1;    // Enable transmitter
    RC1STAbits.CREN = 1;    // Enable continuous receive
    RC1STAbits.SPEN = 1;    // Enable serial port
    
    // ------------------------------------------
    // 4. Interrupt Configuration
    // ------------------------------------------
    PIE3bits.RC1IE = 1;     // Enable UART RX interrupt
    INTCONbits.PEIE = 1;    // Enable peripheral interrupts
    INTCONbits.GIE = 1;     // Enable global interrupts
}

// ==========================================
// ADC Measurement Function
// ==========================================
uint8_t read_adc(void) {
    // 1. Configure RA5 as output and drive it LOW
    LATAbits.LATA5 = 0;
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
    
    while(1) {
        // Clear Hardware Watchdog Timer at top of loop
        CLRWDT(); 

        // Disable override and telemetry after no RX for 2 seconds.
        if (sw_watchdog < SOFTWARE_WDT_MAX_TICKS) {
            sw_watchdog++;
        } else {
            is_override_on = false;
            is_telemetry_on = false;
        }
        
        uint8_t adc_value = read_adc();
        
        // Enable telemetry if ADC reading > 230.
        if (adc_value > TELEMETRY_THRESHOLD) {
            is_telemetry_on = true;
            sw_watchdog = 0;
        }
        
        if (is_telemetry_on) {
            // Transmit telemetry (raw 8-bit ADC value).
            while (!PIR3bits.TX1IF); // Wait for buffer empty
            TX1REG = adc_value;
        }
        
        // Output with safety cutoff
        DAC1CON1 = is_override_on ? rx_byte : adc_value;
        OPA1CON0bits.EN = (target_output_val >= SAFETY_CUTOFF_VAL);
        
        // Blocking delay to pace loop.
        __delay_ms(10);
    }
    
    return 0;
}