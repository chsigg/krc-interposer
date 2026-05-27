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

#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

// Clock Source: Internal Oscillator
#pragma config FEXTOSC = OFF          // Disable external oscillator
#pragma config RSTOSC = LFINTOSC      // Use internal 31kHz oscillator
#pragma config CLKOUTEN = OFF         // Disable clock out

// Watchdog Timer: Enabled
#pragma config WDTE = ON         // Enable watchdog timer
#pragma config WDTCPS = WDTCPS_7 // Set WDT divider ratio to 1:4096 (128ms nominal)
#pragma config WDTCWS = WDTCWS_7 // Set WDT window to always open

// Power-up and Code Protect
#pragma config PWRTS = PWRT_OFF // Disable power-up timer
#pragma config MCLRE = EXTMCLR  // MCLR pin is master clear
#pragma config CP = OFF         // Disable code memory protection
#pragma config LVP = ON         // Enable low voltage programming
#pragma config BOREN = ON       // Enable Brown-out Reset for hardware safety

#define _XTAL_FREQ 31000 // 31 kHz CPU Clock

// Constants
#define SOFTWARE_WDT_MAX_TICKS 200 // 2 seconds / 10ms loops
#define PASSTHROUGH_BYTE 0x00
#define OFF_THRESHOLD 5
#define MIN_OVERRIDE_VAL 25
#define WAKEUP_THRESHOLD 230

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// Persistent System State
static uint8_t rx_byte =
    PASSTHROUGH_BYTE;           // Latest valid command byte from Master
static uint8_t current_adc = 0; // Most recent processed analog sample
static uint8_t sw_watchdog = 0; // Interval tracker since last Master packet

uint8_t calculate_parity(uint8_t val) {
  val ^= val >> 4;
  val ^= val >> 2;
  val ^= val >> 1;
  return val & 1;
}

bool check_uart_polled(void) {
  if (RC1STAbits.OERR) {
    RC1STAbits.CREN = 0;
    RC1STAbits.CREN = 1;
  }
  if (!PIR4bits.RC1IF) {
    return false;
  }
  bool err_frame = RC1STAbits.FERR;
  uint8_t p_rx = RC1STAbits.RX9D;
  uint8_t temp_byte = RC1REG;
  if (err_frame || p_rx != calculate_parity(temp_byte)) {
    return false;
  }
  rx_byte = temp_byte;
  sw_watchdog = 0;
  return true;
}

void send_telemetry(void) {
  while (!PIR4bits.TX1IF)
    ;
  TX1STAbits.TX9D = calculate_parity(current_adc);
  TX1REG = current_adc;
}

void update_actuator(uint8_t output_val) {
  // TODO: adjust output_val based on ref range.
  DAC1DATL = output_val;
  DAC1CONbits.OE1 = output_val >= OFF_THRESHOLD;
  DAC1CONbits.REFRNG = output_val >= 128;
}

uint8_t get_target_val(void) {
  if (rx_byte == PASSTHROUGH_BYTE) {
    return current_adc;
  }
  if (sw_watchdog >= SOFTWARE_WDT_MAX_TICKS) {
    return OFF_THRESHOLD;
  }
  return MAX(rx_byte, MIN_OVERRIDE_VAL);
}

void init_hardware(void) {
  // 1. Pin Configuration
  ANSELAbits = (ANSELAbits_t){.ANSELA2 = 1, .ANSELA4 = 1};
  LATA = 0x00;
  WPUA = 0x00;
  TRISAbits = (TRISAbits_t){.TRISA0 = 1, .TRISA2 = 1, .TRISA4 = 1, .TRISA5 = 1};
  ODCONAbits = (ODCONAbits_t){.ODCA1 = 1};  // Open-drain mode.

  RX1PPS = 0x00;
  RA1PPS = 0x13;
  RA0PPS = 0x00;

  // 2. Peripheral Engine Enables
  ADCON0bits = (ADCON0bits_t){.CS = 1, .FM = 0b01}; // Sets ON=0
  ADPCHbits = (ADPCHbits_t){.PCH2 = 1};             // Channel Select RA4
  DAC1CONbits = (DAC1CONbits_t){.EN = 1, .PSS = 0b00};

  // 3. 7750 Baud Generator Configuration (Max speed at 31 kHz LFINTOSC)
  BAUD1CONbits = (BAUD1CONbits_t){.BRG16 = 1};
  TX1STAbits = (TX1STAbits_t){.TXEN = 1, .BRGH = 1, .TX9 = 1};
  RC1STAbits = (RC1STAbits_t){.SPEN = 1, .CREN = 1, .RX9 = 1};
  SP1BRGL = 0;
  SP1BRGH = 0;

  // 4. Disable all interrupts (polling only, no sleep)
  INTCON = 0x00;
  PIE4 = 0x00;

  // 5. Peripheral Module Disable (PMD) to shut down all unused modules
  PMD0bits = (PMD0bits_t){.TMR0MD = 1,
                          .CLKRMD = 1,
                          .IOCMD = 1,
                          .ACTMD = 1,
                          .SCANMD = 1,
                          .CRCMD = 1};
  PMD1bits = (PMD1bits_t){.TMR1MD = 1, .TMR2MD = 1};
  PMD2bits = (PMD2bits_t){
      .CLC1MD = 1, .CLC2MD = 1, .CLC3MD = 1, .PWM2MD = 1, .NCO1MD = 1};
  PMD3bits = (PMD3bits_t){.CM1MD = 1, .CM2MD = 1, .FVRMD = 1};
}

uint8_t read_adc(void) {
  ADCON0bits.ON = 1;
  TRISAbits.TRISA5 = 0;
  __delay_us(50); // Settle rails

  ADCON0bits.GO = 1;
  volatile uint16_t adc_timeout = 1000;
  while (ADCON0bits.GO && --adc_timeout > 0)
    ;

  uint8_t res = (adc_timeout > 0) ? (uint8_t)(ADRES >> 4) : 0;

  TRISAbits.TRISA5 = 1;
  ADCON0bits.ON = 0; // Return to zero-draw state
  return res;
}

int main(void) {
  CLRWDT(); // Reset WDT after startup initialization
  init_hardware();
  current_adc = read_adc();
  uint8_t output_val = 0;

  while (1) {
    CLRWDT();

    // 1. Read ADC first to get fresh dial position
    current_adc = read_adc();

    // 2. Check UART for master commands
    bool msg_arrived = check_uart_polled();

    // Switch to passthrough if knob is in Off position.
    if (current_adc < OFF_THRESHOLD) {
      rx_byte = PASSTHROUGH_BYTE;
    }

    // 3. Process dial / override logic
    uint8_t target_val = get_target_val();

    if (target_val > output_val) {
      output_val += MIN(target_val - output_val, 3);
    }
    if (target_val < output_val) {
      output_val -= MIN(output_val - target_val, 3);
    }

    // 4. Update DAC output
    update_actuator(output_val);

    // 5. Send telemetry if requested or boil trigger engaged
    if (msg_arrived || current_adc > WAKEUP_THRESHOLD) {
      send_telemetry();
    }

    if (sw_watchdog < SOFTWARE_WDT_MAX_TICKS) {
      sw_watchdog++;
    }
  }
  return 0;
}
