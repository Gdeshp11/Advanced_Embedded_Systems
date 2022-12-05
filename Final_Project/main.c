/*
 * Description: Final Project: A measuring system using an ultrasonic sensor will be designed and built
 * that will signal an alarm when a desired measured distance is reached. In addition,
 * leveling system implemented using an accelerometer. When the board is placed on a flat surface
 * it will signal an alarm when its completely level using the x, y, and z directions
 * file name: main.c
 * Created on: Dec 3, 2022
 * Author: Gandhar Deshpande, Owen Heckmann
 */
#include <msp430.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
// Input pin to check identify microcontroller (high if Tx and low if Rx)
#define MC_IDENTIFICATION_PIN BIT5 // p2.5
// Ultrasonic sensor pins
#define TRIGGER_PIN BIT5 // p1.5
#define ECHO_PIN BIT1    // p2.1
// PWM Output pin for speaker
#define PWM_OUT_SPEAKER BIT6 // p1.6
// UART PINS
#define TX_PIN BIT2 // p1.2
#define RX_PIN BIT1 // p1.1
// PORT2 pins for turning on LED digits
#define DIGIT_1 BIT3 // p2.3
#define DIGIT_2 BIT2 // p2.2
#define DIGIT_3 BIT1 // p2.1
#define DIGIT_4 BIT0 // p2.0
// 7 segments B-F of LED connected to pins of port1 and A to port2
#define a BIT4        // p2.4
#define b BIT5        // p1.5
#define c BIT0        // p1.0
#define d BIT3        // p1.3
#define e BIT7        // p1.7
#define f BIT6        // p1.6
#define g BIT4        // p1.4
#define DIGDELAY 3000 // Number of cycles to delay for displaying each digit in
// display_digits
#define DELAY_SEC 1000000 // 1 sec delay for 1MHz
#define NUM_DISTANCE_PRESETS 5

// ADC Input pins for accelerometer
#define ACCL_INPUT_X BIT3 // p1.3
#define ACCL_INPUT_Y BIT4 // p1.4

// buttons
#define PRESET_BUTTON BIT3 // p2.3
#define MODE_BUTTON BIT4   // p2.4

typedef enum
{
    A_SEGMENT = 0,
    B_SEGMENT,
    C_SEGMENT,
    D_SEGMENT,
    E_SEGMENT,
    F_SEGMENT,
    G_SEGMENT,
} LED_SEGMENTS;

typedef enum
{
    DISTANCE_MEASURING_MODE = 0, LEVELLING_MODE
} modes_t;

// distance measuring mode initially
volatile modes_t current_mode = DISTANCE_MEASURING_MODE;

void led_display_num(const unsigned val);
void lit_led_segment(LED_SEGMENTS segment);
void display_digits(unsigned int val);
void display_axis_info(unsigned int x_axis_val, unsigned int y_axis_val);
void uart_init();
void serial_write(const char *fmt, ...);
void serial_rx_interrupt(void);
void timer_interrupt(void);
void gpio_setup_tx(void);
void gpio_setup_rx(void);
void timer_setup(void);
void generate_trigger_signal(void);
void buzzer_distance_mode(const unsigned int cm);
void buzzer_levelling_mode(void);
void execute_tx();
void execute_rx();
void button_interrupt(void);
void init_buttons(void);
void update_distance_preset(void);
void show_presets(unsigned int preset_distance);
void configure_adc();
void read_adc(unsigned int *x_axis, unsigned int *y_axis);
unsigned int read_adc_running_average_filter(unsigned int raw_adc_val,unsigned int oldaverage,unsigned int weight);

volatile int rising_edge_value, falling_edge_value;
volatile int diff;
volatile unsigned int i = 0;
volatile int distance;
volatile unsigned char rxDataBytesCounter = 0;
volatile unsigned int distance_rx = 0, x_axis_val = 0, y_axis_val = 0;
volatile char rxBuf[25];
unsigned int distance_presets[NUM_DISTANCE_PRESETS] = { 5, 25, 50, 100, 250 }; //distances in cm
unsigned int current_distance_preset_index = 0;
unsigned int current_distance_preset = 5;
unsigned int adc_values[3];
volatile unsigned char show_preset_flag = 0;

void main(void)
{
    WDTCTL = WDTPW | WDTHOLD; // stop watchdog timer
    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;
    P2DIR &= ~MC_IDENTIFICATION_PIN; // set p2.5 as input
    // for UART Tx chip
    if (P2IN & MC_IDENTIFICATION_PIN)
    {
        // execute Tx code
        execute_tx();
    }
    // for UART Rx chip
    else if (!(P2IN & MC_IDENTIFICATION_PIN))
    {
        // execute Rx code
        execute_rx();
    }
}
//******************************************************************************
// Module execute_tx(), Last Revision date 11/16/2022, by Gandhar
// Set of main functions for the transmitter chip
// Sets up GPIO, Timer, and UART Pins
// Enables Interrupts
// Measures distance with Ultrasoud, plays buzzer_distance_mode
// Sends the distance to other chip
//*******************************************************************************
void execute_tx()
{
    gpio_setup_tx();
    uart_init();
    timer_setup();
    unsigned int oldaverage_x, oldaverage_y = 0;
    __enable_interrupt();
    while (1)
    {
        switch (current_mode)
        {
        case DISTANCE_MEASURING_MODE:
        {
            generate_trigger_signal();
            __delay_cycles(DELAY_SEC / 3); // 1/3 seconds delay
            // convert distance to cm
            distance = diff / 58;
            // play alarm on speaker
            buzzer_distance_mode(distance);
            serial_write("\r\n#distance:%d", distance);
            break;
        }
        case (LEVELLING_MODE):
        {
//            TA0CCR0 = 0;
            unsigned int accl_x = 0, accl_y = 0;
            read_adc(&accl_x, &accl_y);
            accl_x = read_adc_running_average_filter(accl_x,oldaverage_x,10);
            accl_y = read_adc_running_average_filter(accl_y,oldaverage_y,10);
            oldaverage_x = accl_x;
            oldaverage_y = accl_y;
            serial_write("\r\n#level x:%d, y:%d", accl_x, accl_y);
            if(((abs(accl_x - 490)>=0) && (abs(accl_x - 490)<=10)) && (abs(accl_y - 490)>=0 && abs(accl_y-490)<=10))
            {
                buzzer_levelling_mode();
            }
            else
            {
                TA0CCR0 = 0;
            }
            break;
        }
        default:
            break;
        }
    }
}
//******************************************************************************
// Module execute_rx(), Last Revision date 11/16/2022, by Gandhar
// Set of main functions for the receiver/Display chip
// Sets up Uart and Gpio pins
// Enables Interrupts
// Displays the distance that it receives
//*******************************************************************************
void execute_rx()
{
    // need to initialize uart before enabling uart interrupt otherwise interrupt
    // doesn't work
    uart_init();
    gpio_setup_rx();
    __enable_interrupt();
    // show first preset distance in beginning
    show_presets(distance_presets[0]);
    show_presets(distance_presets[1]);
    show_presets(distance_presets[2]);
    show_presets(distance_presets[3]);
    show_presets(distance_presets[4]);
    while (1)
    {
        switch (current_mode)
        {
        case DISTANCE_MEASURING_MODE:
        {
            display_digits(distance_rx);
            break;
        }
        case LEVELLING_MODE:
        {
            display_axis_info(abs(x_axis_val-490), abs(y_axis_val-490));
//            display_digits(x_axis_val);
//            __delay_cycles(DELAY_SEC*3);
//            display_digits(y_axis_val);
            break;
        }
        default:
            break;
        }
    }
}
//******************************************************************************
// Module gpio_setup_tx(), Last Revision date 11/16/2022, by Gandhar
// Sets up GPIO pins for Tx pin as defined as global variables
//*******************************************************************************
void gpio_setup_tx(void)
{
    // Timer P2SEL
    P1DIR = TRIGGER_PIN | PWM_OUT_SPEAKER;
    P2SEL = ECHO_PIN;
    // UART,PWM P1SEL
    P1SEL = TX_PIN | RX_PIN | PWM_OUT_SPEAKER;
    P1SEL2 = TX_PIN | RX_PIN;
    // Set TRIGGER (P1.6) pin to LOW initially
    P1OUT &= ~TRIGGER_PIN;
    configure_adc();
    init_buttons();
}

//******************************************************************************
// Module gpio_setup_tx(), Last Revision date 11/16/2022, by Gandhar
// Sets up GPIO pins for Rx pin as defined at beginning of program
// As initializes UART Pin Interrupt
//*******************************************************************************
void gpio_setup_rx(void)
{
    P2DIR |= DIGIT_1 + DIGIT_2 + DIGIT_3 + DIGIT_4 + a;
    P1DIR |= b + c + d + e + f + g;
    P1SEL = TX_PIN | RX_PIN;
    P1SEL2 = TX_PIN | RX_PIN;
    P2SEL &= ~BIT6;                                    // enable p2.6 as gpio
    P2SEL &= ~BIT7;                                    // enable p2.7 as gpio
    P2OUT &= ~(DIGIT_1 + DIGIT_2 + DIGIT_3 + DIGIT_4); // all digits off by default
    UC0IE |= UCA0RXIE;                            // Enable USCI_A0 RX interrupt
}

//******************************************************************************
// Module generate_trigger_signal(),
// Last Revision date 11/17/2022, by Gandhar
// Generates Trigger Signal for Ultrasonic Sensor
// Turns on Trigger Pin, delays, Turns off Trigger pin
//*******************************************************************************
void generate_trigger_signal()
{
    P1OUT |= TRIGGER_PIN;
    _delay_cycles(10);
    P1OUT &= ~TRIGGER_PIN;
}

//******************************************************************************
// Module timer setup(),
// Last Revision date 11/21/2022, by Gandhar
// Initializes Timer, both for Speaker PWM
// And for the Ultrasonic sensor
//*******************************************************************************
void timer_setup(void)
{
    // Timer configuration for generating PWM for speaker using pin p1.6 and timer
    // T0.1
    TA0CTL = TASSEL_2 | MC_1;
    TA0CCR0 = 0;
    TA0CCR1 = 200;
    TA0CCTL1 = OUTMOD_7;
    // Timer configuration for echo pin (capture/compare) p2.1 using timer T1.1
    TA1CTL = TASSEL_2 | MC_2;
    TA1CCTL1 = CAP | CCIE | CCIS_0 | CM_3 | SCS;
}

//******************************************************************************
// interrupt timer_interrupt, Last Revision date 11/16/2022, by Gandhar
// interrupt generated by timer when echo pin receives PWM signal from ultrasonic
// sensor
//*******************************************************************************
#pragma vector = TIMER1_A1_VECTOR
__interrupt void timer_interrupt(void)
{
    // first interrupt when rising edge
    if (i == 0)
    {
        rising_edge_value = TA1CCR1;
        TA1CCTL1 &= ~CCIFG;
        i = 1;
    }
    // second interrupt when falling edge
    else if (i == 1)
    {
        falling_edge_value = TA1CCR1;
        TA1CCTL1 &= ~CCIFG;
        diff = falling_edge_value - rising_edge_value;
        i = 0;
    }
}

//******************************************************************************
// Module Function configure_adc(), Last Revision date 11/8/2022, by Gandhar
// function for initializing UART for pins 1.1 and 1.2
//*******************************************************************************
void uart_init()
{
    UCA0CTL1 |= UCSWRST + UCSSEL_2;
    UCA0BR0 = 104; // settings for 9600 baud
    UCA0BR1 = 0;
    UCA0MCTL = UCBRS_0;
    UCA0CTL1 &= ~UCSWRST;
}

void init_buttons(void)
{
    P2IE |= PRESET_BUTTON + MODE_BUTTON;    // P2.3,4 interrupt enabled
    P2IES |= PRESET_BUTTON + MODE_BUTTON;   // P2.3,4 Hi/lo edge
//    P2REN |= PRESET_BUTTON + MODE_BUTTON;   // Enable Pull Up on SW2 (P2.3,4)
    P2IFG &= ~PRESET_BUTTON + ~MODE_BUTTON; // P2.3,4 IFG cleared
}

#pragma vector = PORT2_VECTOR
__interrupt void button_interrupt(void)
{
    switch (P2IFG)
    {
    case PRESET_BUTTON:
    {
        if(current_mode == DISTANCE_MEASURING_MODE)
        {
            update_distance_preset();
        }
        break;
    }
    case MODE_BUTTON:
    {
        current_mode = (current_mode == DISTANCE_MEASURING_MODE) ?
                        LEVELLING_MODE : DISTANCE_MEASURING_MODE;
        break;
    }
    default:
        break;
    }
    //delay to handle debounce
//    __delay_cycles(100);
    P2IFG = 0;
}

//******************************************************************************
// Module Function configure_adc(), Last Revision date 9/13/2022, by Owen
// function for initializing ADC
//*******************************************************************************
void configure_adc()
{

//    ADC10CTL1 = INCH_3 + ADC10DIV_3; // Channel 7, ADC10CLK/3
    ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON ; // Vcc & Vss as reference,
   //Sample and hold for 64 Clock cycles, ADC on, ADC interrupt enable
//    ADC10AE0 |= ACCL_INPUT_Y; // set p1.7 as adc input pin
}

//******************************************************************************
// Module Function read_adc(), Last Revision date 9/13/2022, by Owen
// Reads ADC_INPUT of ADC and Returns the digital Value as an integer
//*******************************************************************************
void read_adc(unsigned int *x_axis, unsigned int *y_axis)
{
    ADC10CTL1 = ADC10DIV_3 + INCH_3; // Channel 3, ADC10CLK/3
    ADC10AE0 = ACCL_INPUT_X;
    while (ADC10CTL1 & ADC10BUSY);
    ADC10CTL0 |= ENC + ADC10SC; // Sampling and conversion start
    *x_axis = ADC10MEM;         // read x_axis value from ADC
    __delay_cycles(100);
    ADC10CTL1 = ADC10DIV_3 + INCH_4; // channel 4
    ADC10AE0 = ACCL_INPUT_Y;
    while (ADC10CTL1 & ADC10BUSY);
    ADC10CTL0 |= ENC + ADC10SC; // Sampling and conversion start
    *y_axis = ADC10MEM;         // read y_axis value from ADC
}

//******************************************************************************
//Module Function read_adc_running_average_filter(), Last Revision date 9/22/2022, by Owen
//Taking the old average, and the weight, which is set above, calculates the new running average
//The higher the weight, the more oscillations are damped down
//*******************************************************************************
unsigned int read_adc_running_average_filter(unsigned int raw_adc_val,unsigned int oldaverage,
                                             unsigned int weight)
{
    unsigned int average = (oldaverage * weight + raw_adc_val) / (weight + 1); //Simple running average formula
    return average;
}

void update_distance_preset()
{
    current_distance_preset_index++;
    if (current_distance_preset_index >= 5)
    {
        current_distance_preset_index = 0;
    }
    current_distance_preset = distance_presets[current_distance_preset_index];
//    serial_write("\r\n#preset:%d", current_distance_preset);
}

//******************************************************************************
// Module Function serial_write(char *str,...)
// Last Revision date 11/19/2022, by Gandhar
// function for sending string of information to serial console.
// can accept formatted string as argument and then converts it to string.
//*******************************************************************************
void serial_write(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char msg[50];
    vsprintf(msg, fmt, args);
    int i = 0;
    while (msg[i] != 0)
    {
        while (!(IFG2 & UCA0TXIFG))
            ;
        UCA0TXBUF = msg[i];
        i++;
    }
}

//******************************************************************************
// interrupt serial_rx_interrupt, Last Revision date 11/8/2022, by Gandhar
// When a chip configured to listen to UART recieves data, extracts the
// Integer from the data stream, and writes it to the global variable adcValue
//*******************************************************************************
#pragma vector = USCIAB0RX_VECTOR
__interrupt void serial_rx_interrupt(void)
{
    if (IFG2 & UCA0RXIFG)
    {
        __disable_interrupt();
        if (UCA0RXBUF == '\r' && rxDataBytesCounter == 0)
        {
            rxBuf[rxDataBytesCounter++] = UCA0RXBUF;
        }
        else if (rxDataBytesCounter > 0 && rxDataBytesCounter < 25)
        {
            rxBuf[rxDataBytesCounter++] = UCA0RXBUF;
        }
        else if (rxDataBytesCounter >= 25)
        {
            if (sscanf(rxBuf, "\r\n#distance:%d", &distance_rx))
            {
                current_mode = DISTANCE_MEASURING_MODE;
            }
            else if (sscanf(rxBuf, "\r\n#level x:%d, y:%d", &x_axis_val,
                            &y_axis_val))
            {
                current_mode = LEVELLING_MODE;
            }
//            else if (sscanf(rxBuf, "\r\n#preset:%d",&distance_rx))
//            {
//                show_preset_flag = 1;
//            }
            rxDataBytesCounter = 0;
        }
        IFG2 &= ~UCA0RXIFG;
        __enable_interrupt();
    }
}

//******************************************************************************
// Module Function led_display_num(), Last Revision date 9/22/2022, by Gandhar
// Taking in a value, lights all of the segments required so that the value can be
// displayed
// Requires that the correct pins be declared on the top
//*******************************************************************************
void led_display_num(const unsigned val)
{
    // turn off all segments first
    P1OUT |= b + c + d + e + f + g;
    P2OUT |= a;
    // For each number, light the needed segments
    // using program lit_let_segment
    switch (val)
    {
    case 0:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(B_SEGMENT);
        lit_led_segment(C_SEGMENT);
        lit_led_segment(D_SEGMENT);
        lit_led_segment(E_SEGMENT);
        lit_led_segment(F_SEGMENT);
        break;
    }
    case 1:
    {
        lit_led_segment(B_SEGMENT);
        lit_led_segment(C_SEGMENT);
        break;
    }
    case 2:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(B_SEGMENT);
        lit_led_segment(D_SEGMENT);
        lit_led_segment(E_SEGMENT);
        lit_led_segment(G_SEGMENT);
        break;
    }
    case 3:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(B_SEGMENT);
        lit_led_segment(C_SEGMENT);
        lit_led_segment(D_SEGMENT);
        lit_led_segment(G_SEGMENT);
        break;
    }
    case 4:
    {
        lit_led_segment(B_SEGMENT);
        lit_led_segment(C_SEGMENT);
        lit_led_segment(F_SEGMENT);
        lit_led_segment(G_SEGMENT);
        break;
    }
    case 5:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(C_SEGMENT);
        lit_led_segment(D_SEGMENT);
        lit_led_segment(F_SEGMENT);
        lit_led_segment(G_SEGMENT);
        break;
    }
    case 6:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(C_SEGMENT);
        lit_led_segment(D_SEGMENT);
        lit_led_segment(E_SEGMENT);
        lit_led_segment(F_SEGMENT);
        lit_led_segment(G_SEGMENT);
        break;
    }
    case 7:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(B_SEGMENT);
        lit_led_segment(C_SEGMENT);
        break;
    }
    case 8:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(B_SEGMENT);
        lit_led_segment(C_SEGMENT);
        lit_led_segment(D_SEGMENT);
        lit_led_segment(E_SEGMENT);
        lit_led_segment(F_SEGMENT);
        lit_led_segment(G_SEGMENT);
        break;
    }
    case 9:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(B_SEGMENT);
        lit_led_segment(C_SEGMENT);
        lit_led_segment(F_SEGMENT);
        lit_led_segment(G_SEGMENT);
        break;
    }
    default:
        lit_led_segment(G_SEGMENT);
        break;
    }
}

//******************************************************************************
// Module Function lit_let_segment(), Last Revision date 9/22/2022, by Gandhar
// Given a segment, turn it on.
//*******************************************************************************
void lit_led_segment(LED_SEGMENTS segment)
{
    switch (segment)
    {
    case A_SEGMENT:
    {
        P2OUT &= ~a;
        break;
    }
    case B_SEGMENT:
    {
        P1OUT &= ~b;
        break;
    }
    case C_SEGMENT:
    {
        P1OUT &= ~c;
        break;
    }
    case D_SEGMENT:
    {
        P1OUT &= ~d;
        break;
    }
    case E_SEGMENT:
    {
        P1OUT &= ~e;
        break;
    }
    case F_SEGMENT:
    {
        P1OUT &= ~f;
        break;
    }
    case G_SEGMENT:
    {
        P1OUT &= ~g;
        break;
    }
    default:
        break;
    }
}

void show_presets(unsigned int preset_distance)
{
    display_digits(preset_distance);
    __delay_cycles(DELAY_SEC * 2);
    show_preset_flag = 0;
}

//******************************************************************************
// Module Function display_digits(), Last Revision date 9/22/2022, by Gandhar
// Given a number to display, turns on the digits in display that need to be turned
// on
// Then displays the digit, delaying an amount of time set in macros to improve
// persistence of vision
//*******************************************************************************
void display_digits(unsigned int val)
{
    unsigned int digit = 0;
    unsigned int number = val;
    if (val >= 0 && val <= 9)
    {
        // If the number is less than 10, turn on
        // first digit and display the value
        digit = number % 10;
        P2OUT = DIGIT_4;
        led_display_num(digit);
        __delay_cycles(DIGDELAY);
    }
    else if (val >= 10 && val <= 99)
    {
        // If the number is more than 10, but less than 99
        // Turn on 1s place digit, display value, and delay
        // Then extract 10s place digit and repeat
        digit = number % 10;
        P2OUT = DIGIT_4;
        led_display_num(digit);
        digit = (number / 10) % 10;
        __delay_cycles(DIGDELAY);
        P2OUT = DIGIT_3;
        led_display_num(digit);
        __delay_cycles(DIGDELAY);
    }
    else if (val >= 100 && val <= 999)
    {
        // If the number is more than 10, but less than 99
        // Turn on 1s place digit, display value, and delay
        // Then extract 10s place digit and repeat
        // Then extract 1000s place digit and repeat again
        digit = number % 10;
        P2OUT = DIGIT_4;
        led_display_num(digit);
        __delay_cycles(DIGDELAY);
        digit = (number / 10) % 10;
        P2OUT = DIGIT_3;
        led_display_num(digit);
        __delay_cycles(DIGDELAY);
        digit = (number / 100) % 10;
        P2OUT = DIGIT_2;
        led_display_num(digit);
        __delay_cycles(DIGDELAY);
    }
    else if (val >= 1000 && val <= 9999)
    {
        // Do the same process as above, but for all 4 digits.
        digit = number % 10;
        P2OUT = DIGIT_4;
        led_display_num(digit);
        __delay_cycles(DIGDELAY);
        digit = (number / 10) % 10;
        P2OUT = DIGIT_3;
        led_display_num(digit);
        __delay_cycles(DIGDELAY);
        digit = (number / 100) % 10;
        P2OUT = DIGIT_2;
        led_display_num(digit);
        __delay_cycles(DIGDELAY);
        digit = (number / 1000) % 10;
        P2OUT = DIGIT_1;
        led_display_num(digit);
        __delay_cycles(DIGDELAY);
    }
}

void display_axis_info(unsigned int x_axis_val, unsigned int y_axis_val)
{
    if (x_axis_val >= 0 && x_axis_val <= 9)
    {
        // If the number is less than 10, turn on
        // first digit and display the value
        P2OUT = DIGIT_2;
        led_display_num(x_axis_val);
        __delay_cycles(DIGDELAY);
    }
    else if (x_axis_val >= 10 && x_axis_val <= 99)
    {
        P2OUT = DIGIT_2;
        led_display_num(x_axis_val % 10);
        __delay_cycles(DIGDELAY);
        P2OUT = DIGIT_1;
        led_display_num((x_axis_val / 10) % 10);
        __delay_cycles(DIGDELAY);
    }

    if (y_axis_val >= 0 && y_axis_val <= 9)
    {
        // If the number is less than 10, turn on
        // first digit and display the value
        P2OUT = DIGIT_4;
        led_display_num(x_axis_val);
        __delay_cycles(DIGDELAY);
    }
    else if (y_axis_val >= 10 && y_axis_val <= 99)
    {
        P2OUT = DIGIT_4;
        led_display_num(y_axis_val % 10);
        __delay_cycles(DIGDELAY);
        P2OUT = DIGIT_3;
        led_display_num((y_axis_val / 10) % 10);
        __delay_cycles(DIGDELAY);
    }
}

//******************************************************************************
// Module Function void buzzer_distance_mode(unsigned int cm),
// Last Revision date 11/16/2022, by Owen
// Given a distance in cm, activates the buzzer_distance_mode with a certain pitch.
// Lower distances have higher pitches
//********************************************************************************
void buzzer_distance_mode(const unsigned int cm)
{
    if (cm == 5 && current_distance_preset == 5)
    {
        // Set the period in the Timer A0
        // To 300.
        TA0CCR0 = 300;
    }
    else if ((cm == 25) && (current_distance_preset == 25))
    {
        // Set the period in the Timer A0
        // To 600.
        TA0CCR0 = 600;
    }
    else if ((cm == 50) && (current_distance_preset == 50))
    {
        // Set the period in the Timer A0
        // To 1250.
        TA0CCR0 = 1250;
    }
    else if ((cm == 100) && (current_distance_preset == 100))
    {
        // Set the period in the Timer A0
        // To 2500.
        TA0CCR0 = 2500;
    }
    else if ((cm == 250) && (current_distance_preset == 250))
    {
        // Set the period in the Timer A0
        // To 20000.
        TA0CCR0 = 20000;
    }
    else
    {
        TA0CCR0 = 0;
    }
}

void buzzer_levelling_mode()
{
    TA0CCR0 = 600;
}
