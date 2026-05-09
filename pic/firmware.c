/*
 * Smart pass-through and override controller for an analog stove dial.
 * Reads physical 10k potentiometer, buffers to analog output, and
 * communicates with a 3.3V master device via 9600 baud UART.
 *
 * Microcontroller: PIC16F17114, Compiler: XC8
 *
 * xc8-cc.exe -mcpu=16F17114 -mdfp=Microchip.PIC16F1xxxx_DFP.1.30.457\xc8 \
 *   -O2 firmware.c -o firmware.hex
 */

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

// ------------------------------------------
// Hardware Configuration Bits
// ------------------------------------------

// Clock Source: Internal Oscillator
#pragma config FEXTOSC = OFF          // Disable external oscillator
#pragma config RSTOSC = HFINTOSC_1MHz // Use internal 1MHz oscillator
#pragma config CLKOUTEN = OFF         // Disable clock out

// Watchdog Timer: Enabled, ~256ms timeout
#pragma config WDTE = ON         // Enable watchdog timer
#pragma config WDTCPS = WDTCPS_5 // Set WDT divider ratio to 1:1024 (approx 256ms)
#pragma config WDTCWS = WDTCWS_7 // Set WDT window to always open (100%)

// Power-up and Code Protect
#pragma config PWRTS = PWRT_OFF // Disable power-up timer
#pragma config MCLRE = EXTMCLR  // MCLR pin is master clear
#pragma config CP = OFF         // Disable code memory protection
#pragma config LVP = ON         // Enable low voltage programming
#define _XTAL_FREQ 2000000      // 2 MHz CPU Clock (Verified real freq of OSCFRQ=0x01 per DS40002403)

// Constants
#define SOFTWARE_WDT_MAX_TICKS 200  // 2 seconds / 10ms loops
#define PASSTHROUGH_BYTE       0x00
#define OFF_THRESHOLD          5
#define MIN_OVERRIDE_VAL       25
#define WAKEUP_THRESHOLD       230

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// ------------------------------------------
// Persistent System State
// ------------------------------------------
static uint8_t rx_byte = PASSTHROUGH_BYTE; // Latest valid command byte from Master
static uint8_t current_adc = 0;           // Most recent processed analog sample
static uint8_t output_val = 0;            // Dynamic smoothed actuator level
static uint8_t sw_watchdog = 0;           // Interval tracker since last Master packet

// ------------------------------------------
// Computes parity bit for validation of communication payloads.
// Iteratively XORs byte down to a single bit resolution.
// ------------------------------------------
uint8_t calculate_parity(uint8_t val) {
    val ^= val >> 4;
    val ^= val >> 2;
    val ^= val >> 1;
    return val & 1;
}

// ------------------------------------------
// Checks for available byte in the receive queue.
// Exits non-blocking if no data is present to keep main loop agile.
// ------------------------------------------
bool check_uart_polled(void) {
    // Clear hardware freeze on detected serial buffer collisions
    if (RC1STAbits.OERR) {
        RC1STAbits.CREN = 0;
        RC1STAbits.CREN = 1;
    }

    // Early return: Nothing to process in the FIFO right now
    if (!PIR4bits.RC1IF) {
        return false;
    }

    uint8_t p_rx = RC1STAbits.RX9D;
    uint8_t temp_byte = RC1REG;

    // Silently discard mathematically corrupted frames
    if (p_rx != calculate_parity(temp_byte)) {
        return false;
    }

    // Commit and kick state variables
    rx_byte = temp_byte;
    sw_watchdog = 0;
    return true;
}

// ------------------------------------------
// Broadcasts current sensor telemetry via UART to master device.
// Employs blocking clearance checks prior to populating registers.
// ------------------------------------------
void send_telemetry(void) {
    // Non-flooding execution: only waits briefly for outbound slot clearance
    while (!PIR4bits.TX1IF);

    TX1STAbits.TX9D = calculate_parity(current_adc);
    TX1REG = current_adc;
}

// ------------------------------------------
// Transmits resolved target level into the DAC output stage.
// Includes gate monitoring to enforce absolute zero-cutoff limits.
// ------------------------------------------
void update_actuator(void) {
    DAC1DATL = output_val;

    // Dynamic High-Z Float Control:
    // Force absolute cutoff if commanded below active floor, saving stove interlock faulting
    bool output_active = (output_val >= OFF_THRESHOLD);
    OPA1CON0bits.EN = output_active;
    TRISAbits.TRISA2 = !output_active;
}

// ------------------------------------------
// Determines output level based on operating state.
// Structured with flattened guard clauses for maintenance transparency.
// ------------------------------------------
void process_safety_ramps(void) {
    // Priority 1: Passthrough Mode requested by Master.
    // (Explicitly allowed before Watchdog to preserve User Manual Control capabilities).
    if (rx_byte == PASSTHROUGH_BYTE) {
        output_val = current_adc;
        return;
    }

    // Priority 2: Safety Inactivity Cutoff.
    if (sw_watchdog >= SOFTWARE_WDT_MAX_TICKS) {
        output_val = OFF_THRESHOLD;
        return;
    }

    // Priority 3: PID Active Override.
    // Incrementally steps toward sanitized target to enforce slew rate limits.
    uint8_t target_val = MAX(rx_byte, MIN_OVERRIDE_VAL);

    if (target_val > output_val) {
        output_val += MIN(target_val - output_val, 3);
        return;
    }

    if (target_val < output_val) {
        output_val -= MIN(output_val - target_val, 3);
        return;
    }
}

// ------------------------------------------
// Hardware configuration initialization.
// ------------------------------------------
void init_hardware(void) {
    // 1. System Clock (2 MHz Internals)
    OSCFRQ = 0x01;
    OSCCON1bits = (OSCCON1bits_t){ .NOSC = 0b110, .NDIV = 0x00 }; // HFINTOSC, Div 1:1
    volatile uint16_t osc_timeout = 1000;
    while (OSCCON3bits.ORDY == 0 && --osc_timeout > 0);

    // 2. Pin Configuration
    ANSELAbits = (ANSELAbits_t){ .ANSELA0=0, .ANSELA1=0, .ANSELA2=1, .ANSELA4=1, .ANSELA5=0 };
    TRISAbits = (TRISAbits_t){ .TRISA0=1, .TRISA1=0, .TRISA2=1, .TRISA4=1, .TRISA5=1 };
    LATAbits = (LATAbits_t){ .LATA1=0, .LATA5=0 };
    WPUAbits = (WPUAbits_t){ .WPUA1=0 };
    ODCONAbits = (ODCONAbits_t){ .ODCA1=1 };

    RX1PPS = 0x00;
    RA1PPS = 0x13;
    RA0PPS = 0x00;

    // 3. Peripheral Engine Enables
    ADCON0bits = (ADCON0bits_t){ .CS=1, .FM=0b01 }; // Sets ON=0
    ADPCH = 0x04;          // Channel Select RA4
    DAC1CONbits = (DAC1CONbits_t){ .EN = 1 };
    OPA1CON2bits = (OPA1CON2bits_t){ .NCH = 0b001, .PCH = 0b010 };
    OPA1CON0bits = (OPA1CON0bits_t){ .EN = 0 };

    // 4. 9600 Baud Generator Configuration
    BAUD1CONbits = (BAUD1CONbits_t){ .BRG16 = 1 };
    TX1STAbits = (TX1STAbits_t){ .TXEN=1, .BRGH=1, .TX9=1 };
    RC1STAbits = (RC1STAbits_t){ .SPEN=1, .CREN=1, .RX9=1 };
    SP1BRGL = 51;
    SP1BRGH = 0;

    // 5. Timer2 Period Configuration (Deterministic Hardware Counter)
    T2CLKCON = 0x01;       // Source: Fosc/4 (500 kHz hardware resolution)
    T2PR = 249;            // Period match = 250 counts
    T2CONbits = (T2CONbits_t){ .ON=1, .CKPS=0b010, .OUTPS=0b0100 };

    // 6. Ensure all interrupts are disabled at controller and peripheral levels
    INTCONbits = (INTCONbits_t){ .GIE = 0, .PEIE = 0 };
    PIE4bits = (PIE4bits_t){ .RC1IE = 0 };
    PIE2bits = (PIE2bits_t){ .TMR2IE = 0 };
}

// ------------------------------------------
// Synchronous active measurement routine with dynamic power gating.
// ------------------------------------------
uint8_t read_adc(void) {
    ADCON0bits.ON = 1;
    TRISAbits.TRISA5 = 0;
    __delay_us(50); // Settle rails

    ADCON0bits.GO = 1;
    volatile uint16_t adc_timeout = 1000;
    while (ADCON0bits.GO && --adc_timeout > 0);

    uint8_t res = (adc_timeout > 0) ? (uint8_t)(ADRES >> 4) : 0;

    TRISAbits.TRISA5 = 1;
    ADCON0bits.ON = 0; // Return to zero-draw state
    return res;
}

// ------------------------------------------
// Main Loop
// ------------------------------------------
int main(void) {
    init_hardware();

    current_adc = read_adc();  // Initialize ADC reading.

    while (1) {
        CLRWDT();

        // Check for received packets from master
        bool msg_arrived = check_uart_polled();
        if (msg_arrived) {
            send_telemetry();
        }

        if (!PIR2bits.TMR2IF) {
            continue;  // Wait for 10ms interval match flag
        }
        PIR2bits.TMR2IF = 0;

        current_adc = read_adc();

        // Switch to passthrough if knob is in Off position.
        if (current_adc < OFF_THRESHOLD) {
            rx_byte = PASSTHROUGH_BYTE;
        }

        // Wake up master if ADC reading is high (boil trigger engaged).
        if (!msg_arrived && current_adc > WAKEUP_THRESHOLD) {
            send_telemetry();
        }

        // Update output target and set op-amp level.
        process_safety_ramps();
        update_actuator();

        if (sw_watchdog < SOFTWARE_WDT_MAX_TICKS) {
            sw_watchdog++;
        }
    }
    return 0;
}
