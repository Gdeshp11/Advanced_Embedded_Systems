/*
 *  uart_transmitter.c
 *  Created on: Oct 31, 2022
 *  Author: Gandhar Deshpande, Owen Heckmann
 */

#include <msp430.h>
#include <string.h>
#include <stdio.h>

// PORT2 pins for turning on LED digits
#define DIGIT_1 0x08  //p2.3
#define DIGIT_2 0x04  //p2.2
#define DIGIT_3 0x02  //p2.1
#define DIGIT_4 0x01  //p2.0

// 7 segments B-F of LED connected to pins of port1 and  A to port2
#define a 0x10    //p2.4
#define b 0x20    //p1.5
#define c 0x08    //p1.3
#define d 0x02    //p1.1
#define e 0x01    //p1.0
#define f 0x40    //p1.6
#define g 0x10    //p1.4

//Digit dots on LED
#define DOT BIT5 //p2.5

#define ADC_INPUT BIT3 //p1.7 for ADC
#define LED_DIGITS 4

#define DIGDELAY 1000 //Number of cycles to delay for displaying each digit in display_digits

#define TXLED BIT0

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

#pragma pack(1)

typedef union
{
    unsigned char txBuf[5];

    struct {
    unsigned char startByte;
    unsigned char dataLength;
    unsigned char dataByte1;
    unsigned char dataByte2;
    unsigned char endByte;
    };

} txData;

//p1.7 for ADC
//p2.4 for LED pin 11 which is for A

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
//Module Function read_adc_mean_filtered(), Last Revision date 9/22/2022, by Owen
//Given a number, reads the ADC a certain number of times, and returns the unweighted average
//Ultimately not used do to combination of time and oscillation
//*******************************************************************************
unsigned int read_adc_mean_filtered(volatile unsigned int number);

//******************************************************************************
//Module Function read_adc_median_filtered(), Last Revision date 9/22/2022, by Owen
//Given a number, reads the ADC a certain number of times, and returns the median of them
//Ultimately not used do to combination of time and oscillation
//*******************************************************************************
unsigned int read_adc_median_filtered(volatile unsigned int number);

//******************************************************************************
//Module Function read_adc_running_average_filter(), Last Revision date 9/22/2022, by Owen
//Taking the old average, and the weight, which is set above, calculates the new running average
//The higher the weight, the more oscillations are damped down
//*******************************************************************************
unsigned int read_adc_running_average_filter(unsigned int oldaverage, unsigned int weight);

//******************************************************************************
//Module Function led_display_num(), Last Revision date 9/22/2022, by Gandhar
//Taking in a value, lights all of the segments required so that the value can be displayed
//Idea from https://electronics.stackexchange.com/questions/157977/should-i-always-put-a-low-pass-filter-on-an-adc-input
//Requires that the correct pins be declared on the top
//*******************************************************************************
void led_display_num(unsigned val);

//******************************************************************************
//Module Function lit_let_segment(), Last Revision date 9/22/2022, by Gandhar
//Given a segment, turn it on.
//*******************************************************************************
void lit_led_segment(LED_SEGMENTS segment);

void display_digits(unsigned int val);

void uart_init();

void ser_output(unsigned char *str);

void send_data(unsigned int adc_val)
{
    // txData buf = {
    //     .startByte=0xAA,
    //     .dataLength=0x0A,
    //     .dataByte1=((adc_val >> 8) & 0xFF),
    //     .dataByte2=(adc_val & 0xFF),
    //     .endByte=0xBB
    // };

    // unsigned int adc_val_obtained = buf.dataByte1 << 8 | buf.dataByte2;
    // char char_buf[25];
    // snprintf(char_buf,sizeof(char_buf),"\r\n\nADC val obtained:%d\n\n",adc_val_obtained);
    // ser_output(char_buf);

    unsigned char txBuf_arr[5] = {0xAA, 0x0A, ((adc_val >> 8) & 0xFF), (adc_val & 0xFF), 0xBB};
//    ser_output("\r\n\nHello from Transmitter!!!");
    ser_output(txBuf_arr);
}

void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;       // stop watchdog timer

    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;

    P2DIR |= DIGIT_1 + DIGIT_2 + DIGIT_3 + DIGIT_4 + a + DOT;
    P1DIR |= b + c + d + e + f + g ;

    P1DIR |=  TXLED;

    volatile unsigned int i = 0;
    unsigned int oldaverage = 0;
    P2OUT &= ~(DIGIT_1 + DIGIT_2 + DIGIT_3 + DIGIT_4); //all digits off by default
    configureAdc();
    uart_init();

    unsigned int adc_val = read_adc();
    oldaverage = adc_val;
    display_digits(adc_val); //The problem with Running Average is that it there can be a "ramp up", displaying one value in beginning helps mitigate this
    while (1)
    {
         char txbuf[25];

//        unsigned int adc_val = read_adc_median_filtered();
//        ser_output("\r\n\nADC val:");
        unsigned int adc_val = read_adc();
//         snprintf(txbuf,sizeof(txbuf),"\r\n\nADC val:%d",adc_val);
//        ltoa(adc_val,txbuf,10);
//         ser_output(txbuf);
        send_data(adc_val);
//        adc_val = read_adc_running_average_filter(oldaverage, 10);
//        display_digits(adc_val);
//        oldaverage = adc_val;
        __delay_cycles(10000);
    }
}


void uart_init()
{
    P1SEL = BIT1 | BIT2;
    P1SEL2 = BIT1 | BIT2;
    UCA0CTL1 |= UCSWRST+UCSSEL_2;
    UCA0BR0 = 104;  //settings for 9600 baud
    UCA0BR1 = 0;
    UCA0MCTL = UCBRS_0;
    UCA0CTL1 &= ~UCSWRST;
}

void ser_output(unsigned char *str){
    P1OUT |= TXLED;
    while(*str != 0) {
        while (!(IFG2&UCA0TXIFG));
        UCA0TXBUF = *str++;
        }
    P1OUT &= ~TXLED;
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
    while (ADC10CTL1 & ADC10BUSY)
        ;
    ADC10CTL0 |= ENC + ADC10SC;         // Sampling and conversion start
    unsigned int ADC_value = ADC10MEM; // Assigns the value held in ADC10MEM to the integer called ADC_value
    return ADC_value;
}
//******************************************************************************
//Module Function read_adc_mean_filtered(), Last Revision date 9/22/2022, by Owen
//Given a number, reads the ADC a certain number of times, and returns the unweighted average
//Ultimately not used do to combination of time and oscillation
//*******************************************************************************
unsigned int read_adc_mean_filtered(volatile unsigned int number) {
    unsigned int values = 0;
    volatile unsigned int i;
    for(i = number; i > 0; i--) {
        values += read_adc();
    }
    return values / number;
}

//******************************************************************************
//Module Function read_adc_median_filtered(), Last Revision date 9/22/2022, by Owen
//Given a number, reads the ADC a certain number of times, and returns the median of them
//Ultimately not used do to combination of time and oscillation
//*******************************************************************************
unsigned int read_adc_median_filtered(volatile unsigned int number)
{
    unsigned int arr[number];
    volatile unsigned int i,j,tmp;

    for(i = number; i > 0; i--) { //reads ADC certain number of times
        arr[i] = read_adc();
    }

    for(i=0;i<number;i++) //Algorithm taken from robotics class last semester
    {                     //Sorts the array by size
        for(j=0; j < number; j++)
        {
            if(arr[j] > arr[i]) {
                tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
            }
        }
    }
    return arr[number / 2]; //returns the middle value in the array, which is the Median
}
//******************************************************************************
//Module Function read_adc_running_average_filter(), Last Revision date 9/22/2022, by Owen
//Taking the old average, and the weight, which is set above, calculates the new running average
//The higher the weight, the more oscillations are damped down
//*******************************************************************************
unsigned int read_adc_running_average_filter(unsigned int oldaverage, unsigned int weight) {
    unsigned int average;
    unsigned int value = read_adc();
    average =  (oldaverage * weight + value)/(weight + 1); //Simple running average formula
    return average;
}

//******************************************************************************
//Module Function led_display_num(), Last Revision date 9/22/2022, by Gandhar
//Taking in a value, lights all of the segments required so that the value can be displayed
//Requires that the correct pins be declared on the top
//*******************************************************************************
void led_display_num(unsigned val)
{
    // turn off all segments first
    P1OUT |= b + c + d + e + f + g;
    P2OUT |= a + DOT;
    //For each number, light the needed segments using program lit_let_segment
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
//Module Function lit_let_segment(), Last Revision date 9/22/2022, by Gandhar
//Given a segment, turn it on.
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
//Module Function display_digits(), Last Revision date 9/22/2022, by Gandhar
//Given a number to display, turns on the digits in display that need to be turned on
//Then displays the digit, delaying an amount of time set in macros to improve persistence of vision
//*******************************************************************************
void display_digits(unsigned int val)
{

    unsigned int digit = 0;
    unsigned int number = val;
    volatile unsigned int i;

    if (val >= 0 && val <= 9)
    {
        //If the number is less than 10, turn on first digit and display the value
        digit = number % 10;
        P2OUT = DIGIT_4;
        led_display_num(digit);
    }
    else if (val >= 10 && val <= 99)
    {
        //If the number is more than 10, but less than 99
        //Turn on 1s place digit, display value, and delay
        //Then extract 10s place digit and repeat
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
        //If the number is more than 10, but less than 99
        //Turn on 1s place digit, display value, and delay
        //Then extract 10s place digit and repeat
        //Then extract 1000s place digit and repeat again
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
        //Do the same process as above, but for all 4 digits.
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
        __delay_cycles(10);
        digit = (number / 1000) % 10;
        P2OUT = DIGIT_1;
        led_display_num(digit);
        __delay_cycles(DIGDELAY);

    }

}