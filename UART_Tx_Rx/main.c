/*
 *  LAB-7: Transmit POT ADC Value from one G2553 to another G2553 on serial and display the value on quad digit LED
 *  main.c
 *  Created on: Oct 31, 2022
 *  Author: Gandhar Deshpande, Owen Heckmann
 */

#include <msp430.h>
#include <string.h>
#include <stdio.h>

#define ADC_INPUT BIT3 //p1.3 for ADC

#define INPUTPIN BIT5 //p2.5 for Test input

// PORT2 pins for turning on LED digits
#define DIGIT_1 BIT3  //p2.3
#define DIGIT_2 BIT2  //p2.2
#define DIGIT_3 BIT1  //p2.1
#define DIGIT_4 BIT0  //p2.0

// 7 segments B-F of LED connected to pins of port1 and  A to port2
#define a BIT4    //p2.4
#define b BIT5    //p1.5
#define c BIT0    //p1.0
#define d BIT3    //p1.3
#define e BIT7    //p1.7
#define f BIT6    //p1.6
#define g BIT4    //p1.4

#define DIGDELAY 2000 // Number of cycles to delay for displaying each digit in display_digits

#define RX_DATA_LENGTH 6

volatile unsigned char rxDataBytesCounter = 0, startByteCounter = 0;
volatile unsigned int adcValue = 0;
volatile char testBuf[25];
unsigned char rxBuf[RX_DATA_LENGTH];

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


//******************************************************************************
//Module Function configureAdc(), Last Revision date 9/13/2022, by Owen
// function for initializing ADC
//*******************************************************************************
void configureAdc();

//******************************************************************************
//Module Function read_adc(), Last Revision date 9/13/2022, by Owen
//Reads ADC_INPUT of ADC and Returns the digital Value as an integer
//*******************************************************************************
unsigned int read_adc();

//******************************************************************************
// Module Function led_display_num(), Last Revision date 9/22/2022, by Gandhar
// Taking in a value, lights all of the segments required so that the value can be displayed
// Requires that the correct pins be declared on the top
//*******************************************************************************
void led_display_num(const unsigned val);

//******************************************************************************
// Module Function lit_let_segment(), Last Revision date 9/22/2022, by Gandhar
// Given a segment, turn it on.
//*******************************************************************************
void lit_led_segment(LED_SEGMENTS segment);

//******************************************************************************
// Module Function display_digits(), Last Revision date 9/22/2022, by Gandhar
// Given a number to display, turns on the digits in display that need to be turned on
// Then displays the digit, delaying an amount of time set in macros to improve persistence of vision
//*******************************************************************************
void display_digits(unsigned int val);

//******************************************************************************
//Module Function configureAdc(), Last Revision date 11/8/2022, by Gandhar
// function for initializing UART for pins 1.1 and 1.2
//*******************************************************************************
void uart_init();

//******************************************************************************
//Module Function ser_output(char *str)
//Last Revision date 11/8/2022, by Gandhar
// function for sending string of information to serial console
//*******************************************************************************
void ser_output(char *str);

//******************************************************************************
//Module Function send_data(unsigned int adc_val),
//Last Revision date 11/8/2022, by Gandhar
// function for sending integer across UART
//*******************************************************************************
void send_data(unsigned int adc_val);

//******************************************************************************
//interrupt serial_rx_interrupt, Last Revision date 11/8/2022, by Gandhar
//When a chip configured to listen to UART recieves data, extracts the
//Integer from the data stream, and writes it to the global variable adcValue
//*******************************************************************************
void serial_rx_interrupt();

void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;       // stop watchdog timer

    P2DIR &= ~INPUTPIN;               // Pin is an input

    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;

    uart_init();

    if ((P2IN & INPUTPIN)) //On if not connected to ground, for Transmitter Chip
    {
        configureAdc();

        unsigned int old_adc_val = 0;

        while (1)
        {
            unsigned int new_adc_val = read_adc();

            if ((new_adc_val - old_adc_val > 1)
                    || (old_adc_val - new_adc_val > 1))
            {
                send_data(new_adc_val);
                old_adc_val = new_adc_val;
            }


        }

    }

    else if (!(P2IN & INPUTPIN)) //Off if connected to ground, for Receiver Chip
    {
        P2DIR |= DIGIT_1 + DIGIT_2 + DIGIT_3 + DIGIT_4 + a;
        P1DIR |= b + c + d + e + f + g;
        P2SEL &= ~BIT6;  //enable p2.6 as gpio
        P2SEL &= ~BIT7;  //enable p2.7 as gpio
        P2OUT &= ~(DIGIT_1 + DIGIT_2 + DIGIT_3 + DIGIT_4); // all digits off by default
        UC0IE |= UCA0RXIE; // Enable USCI_A0 RX interrupt

        __enable_interrupt();

        while (1)
        {
            display_digits(adcValue);

        }
    }
}

void uart_init()
{
    P1SEL = BIT1 | BIT2;
    P1SEL2 = BIT1 | BIT2;
    UCA0CTL1 |= UCSWRST + UCSSEL_2;
    UCA0BR0 = 104;  //settings for 9600 baud
    UCA0BR1 = 0;
    UCA0MCTL = UCBRS_0;
    UCA0CTL1 &= ~UCSWRST;

}

void send_data(unsigned int adc_val)
{
    char txBuf_arr[20];
    sprintf(txBuf_arr, "\r\n#adc_val:%d", (adc_val / 2) * 2);
    ser_output(txBuf_arr);
}

void ser_output(char *str)
{
    while (*str != 0)
    {
        while (!(IFG2 & UCA0TXIFG))
            ;
        UCA0TXBUF = *str++;
    }
}

void configureAdc()
{
    ADC10CTL1 = INCH_3 + ADC10DIV_3;         // Channel 7, ADC10CLK/3
    ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON + ADC10IE; // Vcc & Vss as reference, Sample and hold for 64 Clock cycles, ADC on, ADC interrupt enable
    ADC10AE0 |= ADC_INPUT;                         // set p1.7 as adc input pin
}

unsigned int read_adc()
{
    while (ADC10CTL1 & ADC10BUSY);
    ADC10CTL0 |= ENC + ADC10SC;         // Sampling and conversion start
    unsigned int ADC_value = ADC10MEM; // Assigns the value held in ADC10MEM to the integer called ADC_value
    return ADC_value;
}

#pragma vector = USCIAB0RX_VECTOR
__interrupt void serial_rx_interrupt(void)
{
    //When the Interrupt Flag is tripped
    if (IFG2 & UCA0RXIFG)
    {
        __disable_interrupt();

        //Start Reading from UART
        if (UCA0RXBUF == '\r' && rxDataBytesCounter == 0)
        {
            //Put information into the Buffer
            testBuf[rxDataBytesCounter++] = UCA0RXBUF;
        }
        //Continue Reading from UART for 25 bytes, which is preset length of the message
        //This was a solution to noise
        else if (rxDataBytesCounter > 0 && rxDataBytesCounter < 25)
        {
            //Put information into the Buffer
            testBuf[rxDataBytesCounter++] = UCA0RXBUF;
        }
        //When the UART message has reached the correct length
        else if (rxDataBytesCounter >= 25)
        {
            //Read the Buffer to adcValue
            sscanf(testBuf, "\r\n#adc_val:%d", &adcValue);
            rxDataBytesCounter = 0;
        }

        IFG2 &= ~UCA0RXIFG;
        __enable_interrupt();
    }
}


void led_display_num(const unsigned val)
{
    // turn off all segments first
    P1OUT |= b + c + d + e + f + g;
    P2OUT |= a;
    // For each number, light the needed segments using program lit_let_segment
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

void display_digits(unsigned int val)
{
    //As per Discussion with Joey 11/7/2022
        if(val <= 16)
        {
            val = 0;
        }

        if(val >= 1000)
        {
            val = 1023;
        }

    unsigned int digit = 0;
    unsigned int number = val;
    volatile unsigned int i;

    if (val >= 0 && val <= 9)
    {
        // If the number is less than 10, turn on first digit and display the value
        digit = number % 10;
        P2OUT = DIGIT_4;
        led_display_num(digit);
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
