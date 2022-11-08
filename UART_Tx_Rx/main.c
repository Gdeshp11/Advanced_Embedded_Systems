/*
 *  LAB-7: Transmit POT ADC Value to another G2553 on serial
 *  uart_transmitter.c
 *  Created on: Oct 31, 2022
 *  Author: Gandhar Deshpande, Owen Heckmann
 */

#include <msp430.h>
#include <string.h>
#include <stdio.h>

#define ADC_INPUT BIT3 //p1.7 for ADC

#define INPUTPIN BIT5 //p1.1 for Test input

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

//#define RXLED BIT6
#define RX_DATA_LENGTH 6
#define START_BYTE 0xFF
#define END_BYTE 0xBB

volatile unsigned char rxDataBytesCounter = 0, startByteCounter = 0;
volatile unsigned int adcValue = 2000;
volatile char testBuf[25];

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

//#pragma pack(1)

//typedef union
//{
//    unsigned char txBuf[5];
//
//    struct {
//    unsigned char startByte;
//    unsigned char dataLength;
//    unsigned char dataByte1;
//    unsigned char dataByte2;
//    unsigned char endByte;
//    };
//
//} txData;

//p1.7 for ADC
//p2.4 for LED pin 11 which is for A

void configureAdc();

unsigned int read_adc();

unsigned char rxBuf[RX_DATA_LENGTH];

void led_display_num(const unsigned val);

void lit_led_segment(LED_SEGMENTS segment);

void display_digits(unsigned int val);

void uart_init();

void ser_output(char *str);

void send_data(unsigned int adc_val);

void serial_rx_interrupt();

void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;       // stop watchdog timer



    P2DIR &= ~INPUTPIN;               // Pin is an input
//    P2REN |= INPUTPIN;

    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;

    uart_init();

    if ((P2IN & INPUTPIN))
    {
        configureAdc();

        unsigned int old_adc_val = 0;

        while (1)
        {
//            char str[25] = {0};

            unsigned int new_adc_val = read_adc();
//            sprintf(str,"\r\n\nADC value:%d",new_adc_val);
//            ser_output(str);
            if ((new_adc_val - old_adc_val > 1)
                    || (old_adc_val - new_adc_val > 1))
            {
                send_data(new_adc_val);
                old_adc_val = new_adc_val;
            }


        }

    }

    else if (!(P2IN & INPUTPIN))
    {
        P2DIR |= DIGIT_1 + DIGIT_2 + DIGIT_3 + DIGIT_4 + a;
        P1DIR |= b + c + d + e + f + g;

        P2SEL &= ~BIT6;  //enable p2.6 as gpio
        P2SEL &= ~BIT7;  //enable p2.7 as gpio

        //    P1DIR |= RXLED;

        P2OUT &= ~(DIGIT_1 + DIGIT_2 + DIGIT_3 + DIGIT_4); // all digits off by default
        UC0IE |= UCA0RXIE; // Enable USCI_A0 RX interrupt
        __enable_interrupt();

        while (1)
        {
//            ser_output("\r\n in second if...");
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
    // txData buf = {
    //     .startByte=0xAA,
    //     .dataLength=0x0A,
    //     .dataByte1=((adc_val >> 8) & 0xFF),
    //     .dataByte2=(adc_val & 0xFF),
    //     .endByte=0xBB
    // };

//    unsigned char txBuf_arr[] = {0xFF, 0xFF, 0x0A, ((adc_val >> 8) & 0xFF), (adc_val & 0xFF), 0xBB};
    char txBuf_arr[20];
    sprintf(txBuf_arr, "\r\n#adc_val:%d", (adc_val / 2) * 2);
//    ser_output("\r\n\nHello from Transmitter!!!");
//     char char_buf[25];
//     unsigned int adc_val_obtained = txBuf_arr[2] << 8 | txBuf_arr[3];
//     snprintf(char_buf,sizeof(char_buf),"\r\n\nADC val obtained:%d\n\n",adc_val_obtained);
//     ser_output(char_buf);
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

//******************************************************************************
//Module Function configureAdc(), Last Revision date 9/13/2022, by Owen
// function for initializing ADC
//*******************************************************************************
void configureAdc()
{
    ADC10CTL1 = INCH_3 + ADC10DIV_3;         // Channel 7, ADC10CLK/3
    ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON + ADC10IE; // Vcc & Vss as reference, Sample and hold for 64 Clock cycles, ADC on, ADC interrupt enable
    ADC10AE0 |= ADC_INPUT;                         // set p1.7 as adc input pin
}

//******************************************************************************
//Module Function read_adc(), Last Revision date 9/13/2022, by Owen
//Reads ADC_INPUT of ADC and Returns the digital Value as an integer
//*******************************************************************************
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
    if (IFG2 & UCA0RXIFG)
    {
        __disable_interrupt();
        //        P1OUT |= RXLED;
        //        ser_output("\n--->DEBUG::in interrupt..");

        if (UCA0RXBUF == '\r' && rxDataBytesCounter == 0)
        {
            //            ser_output("\n--->DEBUG::in first if..");

            testBuf[rxDataBytesCounter++] = UCA0RXBUF;
        }
        else if (rxDataBytesCounter > 0 && rxDataBytesCounter < 25)
        {
            //            ser_output("\n--->DEBUG::in second if..");
            testBuf[rxDataBytesCounter++] = UCA0RXBUF;
        }
        else if (rxDataBytesCounter >= 25)
        {
            //            ser_output("\n--->DEBUG::in third if..");
            sscanf(testBuf, "\r\n#adc_val:%d", &adcValue);
            //            display_digits((adcValue/3)*3);
            rxDataBytesCounter = 0;
        }

        //    if (UCA0RXBUF == START_BYTE && (startByteCounter == 0 | startByteCounter == 1)) // 'Start bytes' received?
        //    {
        //        rxData.rxBuf[rxDataBytesCounter++] = UCA0RXBUF;
        //        startByteCounter++;
        //    }
        //
        //    else if ((rxDataBytesCounter >0) && (rxDataBytesCounter < RX_DATA_LENGTH) && (startByteCounter == 2))
        //    {
        //        rxData.rxBuf[rxDataBytesCounter++] = UCA0RXBUF;
        //    }
        //    else if(rxDataBytesCounter == RX_DATA_LENGTH )
        //    {
        //        // display ADC VALUE ON LED
        //        adcValue = (unsigned int)(rxData.dataByte1 << 8 | rxData.dataByte2);
        //        sprintf(testBuf, "\r\n\nADC VALUE:%d", adcValue);
        //        ser_output(testBuf);
        ////        display_digits(adcValue);
        //        sprintf(testBuf, "\r\n----DEBUG::rxData.dataByte1:%x \t rxData.dataByte2:%x", rxData.dataByte1,rxData.dataByte2);
        //        ser_output(testBuf);
        ////        printf(testBuf, "\r\n----DEBBUG::rxData.dataByte2:%x", rxData.dataByte2);
        ////        ser_output(testBuf);
        //
        ////        sprintf(testBuf,rxData.rxBuf);
        ////        ser_output(testBuf);
        //        rxDataBytesCounter = 0;
        //        startByteCounter = 0;
        //    }

        //    }
        IFG2 &= ~UCA0RXIFG;
        //        P1OUT &= ~RXLED;
        __enable_interrupt();
    }
}

//******************************************************************************
// Module Function led_display_num(), Last Revision date 9/22/2022, by Gandhar
// Taking in a value, lights all of the segments required so that the value can be displayed
// Requires that the correct pins be declared on the top
//*******************************************************************************
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

//******************************************************************************
// Module Function display_digits(), Last Revision date 9/22/2022, by Gandhar
// Given a number to display, turns on the digits in display that need to be turned on
// Then displays the digit, delaying an amount of time set in macros to improve persistence of vision
//*******************************************************************************
void display_digits(unsigned int val)
{
    //As per Discussion with Joey 11/7/2022
        if(val <= 16)
        {
            val = 0;
        }

        if(val >= 1005)
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
