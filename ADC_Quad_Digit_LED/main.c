#include <msp430.h>
#include <stdlib.h>

// PORT2 pins for turning on LED digits
#define DIGIT_1 BIT3  //p2.3
#define DIGIT_2 BIT2  //p2.2
#define DIGIT_3 BIT1  //p2.1
#define DIGIT_4 BIT0  //p2.0

// 7 segments B-F of LED connected to pins of port1 and  A to port2
#define a BIT4    //p2.4
#define b BIT5    //p1.5
#define c BIT0    //p1.0
#define d BIT1    //p1.1
#define e BIT7    //p1.7
#define f BIT6    //p1.6
#define g BIT4    //p1.4

//Digit dots on LED
#define DOT BIT5 //p2.5

// using p1.2 for adc input
#define ADC_INPUT BIT2

#define DIGDELAY 2600 //Number of cycles to delay for displaying each digit in display_digits

#define MEDIAN_NUMS 12 // numbers for which median is calculated

// enum for led segments
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
unsigned int read_adc_running_median(unsigned int *adc_val, unsigned int *oldavg, unsigned int *secondoldavg,  unsigned int band);

//******************************************************************************
//Module Function read_adc_running_average_filter(), Last Revision date 9/22/2022, by Owen
//Taking the old average, and the weight, which is set above, calculates the new running average
//The higher the weight, the more oscillations are damped down
//*******************************************************************************
unsigned int read_adc_running_average_filter(unsigned int oldaverage, unsigned int weight);

unsigned int read_adc_mean_filtered(unsigned int oldval,volatile unsigned int number, unsigned int band);

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

//******************************************************************************
//Module Function display_digits(), Last Revision date 9/22/2022, by Gandhar
//Display 4 digit number on quad-led
//*******************************************************************************
void display_digits(unsigned int val);

//******************************************************************************
//Module Function sort_arr(), Last Revision date 10/19/2022, by Gandhar
//sort the array using insertion sort
//*******************************************************************************
unsigned int* sort_arr(unsigned int arr[], unsigned int n);

//******************************************************************************
//Module Function read_adc_median_filtered(), Last Revision date 10/19/2022, by Gandhar
//Given a number, reads the ADC a certain number of times, and returns the median of them
//******************************************************************************
unsigned int read_adc_median_filtered(unsigned int n,unsigned int oldval,unsigned int band);

void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;       // stop watchdog timer

    P2DIR |= DIGIT_1 + DIGIT_2 + DIGIT_3 + DIGIT_4 + a + DOT;
    P1DIR |= b + c + d + e + f + g ;

    P2OUT &= ~(DIGIT_1 + DIGIT_2 + DIGIT_3 + DIGIT_4); //all digits off by default
    configureAdc();
//    unsigned int secondoldavg = read_adc();
//    display_digits(secondoldavg); //The problem with Running Average is that it there can be a "ramp up", displaying one value in beginning helps mitigate this

//    __delay_cycles(DIGDELAY);

    unsigned int oldval = read_adc();
    display_digits(oldval); //The problem with Running Average is that it there can be a "ramp up", displaying one value in beginning helps mitigate this

    while (1)
    {


//        unsigned int adc_val =  read_adc();
//        unsigned int val = read_adc_running_median(&adc_val, &oldavg, &secondoldavg, 10);
//        display_digits(val);
//        secondoldavg = oldavg;
//        oldavg = adc_val;

        unsigned int adc_val = read_adc_median_filtered(MEDIAN_NUMS,oldval,6);
//        unsigned int adc_val = read_adc_mean_filtered(oldval,MEDIAN_NUMS,8);
        display_digits(adc_val);
        oldval = adc_val;

        //oldaverage = adc_val;
    }
}

//******************************************************************************
//Module Function configureAdc(), Last Revision date 9/13/2022, by Owen
// function for initializing ADC
//*******************************************************************************
void configureAdc()
{
    ADC10CTL1 = INCH_2 + ADC10DIV_3;         // Channel 7, ADC10CLK/3
    ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON + ADC10IE; // Vcc & Vss as reference, Sample and hold for 64 Clock cycles, ADC on, ADC interrupt enable
    ADC10AE0 |= ADC_INPUT;                         // set p1.2 as adc input pin
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
//Module Function sort_arr(), Last Revision date 10/19/2022, by Gandhar
//sort the array using insertion sort
//*******************************************************************************
unsigned int* sort_arr(unsigned int arr[], unsigned int n)
{
    volatile unsigned int i = 0;
    volatile unsigned int j = 0;
    volatile unsigned int tmp = 0;

    for (i = 1 ; i <= n-1 ; i++){
       for (j = 1 ; j <= n-i ; j++){
          if (arr[j] <= arr[j+1]){
             tmp = arr[j];
             arr[j] = arr[j+1];
             arr[j+1] = tmp;
          } else
          continue ;
       }
    }
    return arr;
}

//******************************************************************************
//Module Function read_adc_median_filtered(), Last Revision date 10/19/2022, by Gandhar
//Given a number, reads the ADC a certain number of times, and returns the median of them
//******************************************************************************
unsigned int read_adc_median_filtered(unsigned int n,unsigned int oldval,unsigned int band)
{
    volatile unsigned int i = 0;
    unsigned int arr[MEDIAN_NUMS];
    unsigned int* sorted_arr;

    unsigned int adc_val_median = 0;

    for(i=0;i<n;++i)
    {
        arr[i] = read_adc();
        _delay_cycles(1000);
    }

    sorted_arr = sort_arr(arr,n);

    if (n % 2 == 0)
        adc_val_median = (sorted_arr[n/2] + sorted_arr[n/2+1])/2;
    else
        adc_val_median = sorted_arr[n/2 + 1];

    if( (adc_val_median >= oldval + band) || (adc_val_median <= oldval - band))
        return adc_val_median;
    else return oldval;
}

//******************************************************************************
//Module Function read_adc_mean_filtered(), Last Revision date 9/22/2022, by Owen
//Given a number, reads the ADC a certain number of times, and returns the unweighted average
//Ultimately not used do to combination of time and oscillation
//*******************************************************************************
unsigned int read_adc_mean_filtered(unsigned int oldval,volatile unsigned int number, unsigned int band) {
    unsigned int values = 0, average = 0;
    volatile unsigned int i;
    for(i = number; i > 0; i--) {
        values += read_adc();
    }
    average = values / number;

    if( (average >= oldval + band) || (average <= oldval - band))
        return average;
    else return oldval;
}

//******************************************************************************
//Module Function read_adc_running_median(), Last Revision date 10/19/2022, by Owen
//Reads running median of 3 adc values read
//Ultimately not used do to combination of time and oscillation
//*******************************************************************************

unsigned int read_adc_running_median(unsigned int *adc_val, unsigned int *oldavg, unsigned int *secondoldavg, unsigned int band) {

    unsigned int val = 0; // c b a

    if((*adc_val <=  *oldavg) && (*oldavg <= *secondoldavg))  val = *oldavg; // a b c
    else if((*adc_val <=  *secondoldavg) && (*secondoldavg <= *oldavg))  val = *secondoldavg; // a c b
    else if((*oldavg <=  *adc_val) && (*adc_val <= *secondoldavg))  val = *adc_val; //b a c
    else if((*oldavg <=  *secondoldavg) && (*secondoldavg <= *adc_val))  val = *secondoldavg; //b c a
    else if((*secondoldavg <=  *adc_val) && (*adc_val <= *oldavg))  val = *oldavg; // c a b
    else val = *oldavg;

    if( (val >= *oldavg + band) || (val <= *oldavg - band)) {
    return val;
    }
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
        _delay_cycles(DIGDELAY);
    }
    else if (val >= 10 && val <= 99)
    {
        //If the number is more than 10, but less than 99
        //Turn on 1s place digit, display value, and delay
        //Then extract 10s place digit and repeat
        digit = number % 10;
        P2OUT = DIGIT_4;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);
        digit = (number / 10) % 10;
        P2OUT = DIGIT_3;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);

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
        _delay_cycles(DIGDELAY);
        digit = (number / 10) % 10;
        P2OUT = DIGIT_3;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);
        digit = (number / 100) % 10;
        P2OUT = DIGIT_2;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);

    }
    else if (val >= 1000 && val <= 9999)
    {
        //Do the same process as above, but for all 4 digits.
        digit = number % 10;
        P2OUT = DIGIT_4;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);
        digit = (number / 10) % 10;
        P2OUT = DIGIT_3;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);
        digit = (number / 100) % 10;
        P2OUT = DIGIT_2;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);
        digit = (number / 1000) % 10;
        P2OUT = DIGIT_1;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);
    }

}
