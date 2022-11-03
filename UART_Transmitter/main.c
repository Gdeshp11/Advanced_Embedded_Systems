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

#define TXLED BIT0

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




//    unsigned char txBuf_arr[] = {0xFF, 0xFF, 0x0A, ((adc_val >> 8) & 0xFF), (adc_val & 0xFF), 0xBB};
    char txBuf_arr[20];
    sprintf(txBuf_arr,"\r\n#adc_val:%d",(adc_val/2)*2);
//    ser_output("\r\n\nHello from Transmitter!!!");
//     char char_buf[25];
//     unsigned int adc_val_obtained = txBuf_arr[2] << 8 | txBuf_arr[3];
//     snprintf(char_buf,sizeof(char_buf),"\r\n\nADC val obtained:%d\n\n",adc_val_obtained);
//     ser_output(char_buf);
     ser_output(txBuf_arr);
}

void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;       // stop watchdog timer

    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;


    P1DIR |=  TXLED;

    configureAdc();
    uart_init();

    unsigned int old_adc_val = 0;

    while (1)
    {
//         char txbuf[25];

//        unsigned int adc_val = read_adc_median_filtered();
//        ser_output("\r\n\nADC val:");
        unsigned int new_adc_val = read_adc();

        if((new_adc_val-old_adc_val>1) || (old_adc_val-new_adc_val > 1))
        {
            send_data(new_adc_val);
            old_adc_val = new_adc_val;
        }

//         snprintf(txbuf,sizeof(txbuf),"\r\n\nADC val:%d",adc_val);
//        ltoa(adc_val,txbuf,10);
//         ser_output(txbuf);

//        adc_val = read_adc_running_average_filter(oldaverage, 10);
//        display_digits(adc_val);
//        oldaverage = adc_val;
//        __delay_cycles(10000);
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

