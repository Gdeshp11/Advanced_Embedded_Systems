#include <msp430.h>

// PORT2 pins for turning on LED digits
#define DIGIT_1 BIT3  //p2.3
#define DIGIT_2 BIT2  //p2.2
#define DIGIT_3 BIT1  //p2.1
#define DIGIT_4 BIT0  //p2.0

// 7 segments B-F of LED connected to pins of port1 and  A to port2
#define a BIT4    //p2.4
#define b BIT5    //p1.5
#define c BIT6    //p2.6
#define d BIT7    //p2.7
#define e BIT0    //p1.0
#define f BIT6    //p1.6
#define g BIT4    //p1.4

//Digit dots on LED
#define DOT BIT5 //p2.5

#define ADC_INPUT_X BIT0 //p1.0 for ADC
#define ADC_INPUT_Y BIT1 //p1.1 for ADC
#define ADC_INPUT_Z BIT2 //p1.2 for ADC

#define LED_DIGITS 4

#define DIGDELAY 2000 //Number of cycles to delay for displaying each digit in display_digits

// to track which digits of led are on
unsigned char digits_on = 0x00;

unsigned int adc[3];

typedef enum
{
    A_SEGMENT = 0,
    B_SEGMENT,
    C_SEGMENT,
    D_SEGMENT,
    E_SEGMENT,
    F_SEGMENT,
    G_SEGMENT,
    DP,
} LED_SEGMENTS;


// enum for states
typedef enum
{
    rawX,
    rawY,
    rawZ,
    g_x,
    g_y,
    g_z
} STATES;

//enum for numbers/characters on LED
typedef enum
{
    NUM_0,
    NUM_1,
    NUM_2,
    NUM_3,
    NUM_4,
    NUM_5,
    NUM_6,
    NUM_7,
    NUM_8,
    NUM_9,
    CHAR_X,
    CHAR_Y,
    CHAR_Z,
    CHAR_DASH
} LED_CHARS;

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
//Module Function read_adc_running_average_filter(), Last Revision date 9/22/2022, by Owen
//Taking the old average, and the weight, which is set above, calculates the new running average
//The higher the weight, the more oscillations are damped down
//*******************************************************************************
unsigned int read_adc_running_average_filter(unsigned int oldaverage,
                                             unsigned int weight);

//******************************************************************************
//Module Function led_display_num(), Last Revision date 9/22/2022, by Gandhar
//Taking in a value, lights all of the segments required so that the value can be displayed
//Idea from https://electronics.stackexchange.com/questions/157977/should-i-always-put-a-low-pass-filter-on-an-adc-input
//Requires that the correct pins be declared on the top
//*******************************************************************************
void led_display_num(LED_CHARS led_char);

//******************************************************************************
//Module Function lit_let_segment(), Last Revision date 9/22/2022, by Gandhar
//Given a segment, turn it on.
//*******************************************************************************
void lit_led_segment(LED_SEGMENTS segment);

void display_digits(unsigned int val, char axis);

void turn_off_led_segments();

void turn_off_led_digits();

void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;       // stop watchdog timer

    P2DIR |= DIGIT_1 + DIGIT_2 + DIGIT_3 + DIGIT_4 + a + c + d + DOT;
    P1DIR |= b + e + f + g;

    P2SEL &= ~BIT6;  //enable p2.6 as gpio
    P2SEL &= ~BIT7;  //enable p2.7 as gpio

    volatile unsigned int i = 0;
    unsigned int oldaverage = 0;
    P2OUT &= ~(DIGIT_1 + DIGIT_2 + DIGIT_3 + DIGIT_4); //all digits off by default
    configureAdc();
    unsigned int adc_val = read_adc();
    oldaverage = adc_val;
    display_digits(adc_val, 'x'); //The problem with Running Average is that it there can be a "ramp up", displaying one value in beginning helps mitigate this
    while (1)
    {

        digits_on = 0x00;
//        unsigned int adc_val = read_adc_median_filtered();
        unsigned int adc_val = read_adc();
        adc_val = read_adc_running_average_filter(oldaverage, 10);
        display_digits(adc_val, 'x');
        oldaverage = adc_val;


//        P2OUT = DIGIT_1;
//        P2OUT &= ~DOT;
//        _delay_cycles(1000000);
//        P2OUT = DIGIT_2;
//        P2OUT &= ~DOT;
//        _delay_cycles(1000000);
//        P2OUT = DIGIT_3;
//        P2OUT &= ~DOT;
//        _delay_cycles(1000000);
//        P2OUT = DIGIT_4;
//        P2OUT &= ~DOT;
//        _delay_cycles(1000000);
//        display_digits(9,'x');
    }
}

//******************************************************************************
//Module Function configureAdc(), Last Revision date 9/13/2022, by Owen
// function for initializing ADC
//*******************************************************************************
void configureAdc()
{
    ADC10CTL1 = INCH_2 + ADC10DIV_0 + CONSEQ_3 + SHS_0;
    ADC10CTL0 = SREF_0 + ADC10SHT_2 + MSC + ADC10ON; //ADC10IE
    ADC10AE0 = ADC_INPUT_X + ADC_INPUT_Y + ADC_INPUT_Z;
    ADC10DTC1 = 3;
}

//******************************************************************************
//Module Function read_adc(), Last Revision date 9/13/2022, by Owen
//Reads ADC_INPUT of ADC and Returns the digital Value as an integer
//*******************************************************************************
unsigned int read_adc()
{
    ADC10CTL0 &= ~ENC;
    while (ADC10CTL1 & BUSY);
    ADC10CTL0 |= ENC + ADC10SC;
    ADC10SA = (unsigned int)adc;
    return adc[2];
}



//******************************************************************************
//Module Function read_adc_running_average_filter(), Last Revision date 9/22/2022, by Owen
//Taking the old average, and the weight, which is set above, calculates the new running average
//The higher the weight, the more oscillations are damped down
//*******************************************************************************
unsigned int read_adc_running_average_filter(unsigned int oldaverage,
                                             unsigned int weight)
{
    unsigned int average;
    unsigned int value = read_adc();
    average = (oldaverage * weight + value) / (weight + 1); //Simple running average formula
    return average;
}

void turn_off_led_segments()
{
    // turn off all segments first
    P1OUT |= b + e + f + g;
    P2OUT |= a + c + d + DOT;
}

void turn_off_led_digits()
{
    P2OUT &= ~(DIGIT_1 + DIGIT_2 + DIGIT_3 + DIGIT_4);
}
//******************************************************************************
//Module Function led_display_num(), Last Revision date 9/22/2022, by Gandhar
//Taking in a value, lights all of the segments required so that the value can be displayed
//Requires that the correct pins be declared on the top
//*******************************************************************************
void led_display_num(LED_CHARS led_char)
{
    turn_off_led_segments();
    //For each number, light the needed segments using program lit_let_segment
    switch (led_char)
    {
    case NUM_0:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(B_SEGMENT);
        lit_led_segment(C_SEGMENT);
        lit_led_segment(D_SEGMENT);
        lit_led_segment(E_SEGMENT);
        lit_led_segment(F_SEGMENT);
        break;
    }
    case NUM_1:
    {
        lit_led_segment(B_SEGMENT);
        lit_led_segment(C_SEGMENT);
        break;
    }
    case NUM_2:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(B_SEGMENT);
        lit_led_segment(D_SEGMENT);
        lit_led_segment(E_SEGMENT);
        lit_led_segment(G_SEGMENT);
        break;
    }
    case NUM_3:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(B_SEGMENT);
        lit_led_segment(C_SEGMENT);
        lit_led_segment(D_SEGMENT);
        lit_led_segment(G_SEGMENT);
        break;
    }
    case NUM_4:
    {
        lit_led_segment(B_SEGMENT);
        lit_led_segment(C_SEGMENT);
        lit_led_segment(F_SEGMENT);
        lit_led_segment(G_SEGMENT);
        break;
    }
    case NUM_5:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(C_SEGMENT);
        lit_led_segment(D_SEGMENT);
        lit_led_segment(F_SEGMENT);
        lit_led_segment(G_SEGMENT);
        break;
    }
    case NUM_6:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(C_SEGMENT);
        lit_led_segment(D_SEGMENT);
        lit_led_segment(E_SEGMENT);
        lit_led_segment(F_SEGMENT);
        lit_led_segment(G_SEGMENT);
        break;
    }
    case NUM_7:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(B_SEGMENT);
        lit_led_segment(C_SEGMENT);
        break;
    }

    case NUM_8:
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
    case NUM_9:
    {
        lit_led_segment(A_SEGMENT);
        lit_led_segment(B_SEGMENT);
        lit_led_segment(C_SEGMENT);
        lit_led_segment(F_SEGMENT);
        lit_led_segment(G_SEGMENT);
        break;
    }
    case CHAR_X:
    {
        //for displaying char X
    }
    case CHAR_Y:
    {
        //for displaying char Y
    }
    case CHAR_Z:
    {
        //for displaying char Z
    }
    case CHAR_DASH:
    {
        //for displaying sign '-'
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
        P2OUT &= ~c;
        break;
    }
    case D_SEGMENT:
    {
        P2OUT &= ~d;
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
    case DP:
    {
        P2OUT &= ~DOT;
    }
    default:
        break;
    }
}

void display_decimal_point(char axis, unsigned char on_digits)
{
    if (axis == 'x')
    {
        if (!(on_digits & (1 << 3))) //check if digit-1 is turned off and then turn on to show dp
        {
            P2OUT = DIGIT_1;
            turn_off_led_segments();
        }

        lit_led_segment(DP);
    }
    else if (axis == 'y')
    {
        if (!(on_digits & (1 << 2))) //check if digit-2 is turned off and then turn on to show dp
        {
            P2OUT = DIGIT_2;
            turn_off_led_segments();
        }

        lit_led_segment(DP);
    }
    else if (axis == 'z')
    {
        if (!(on_digits & (1 << 1))) //check if digit-3 is turned off and then turn on to show dp
        {
            P2OUT = DIGIT_3;
            turn_off_led_segments();
        }

        lit_led_segment(DP);
    }
    _delay_cycles(DIGDELAY);
}

//******************************************************************************
//Module Function display_digits(), Last Revision date 9/22/2022, by Gandhar
//Given a number to display, turns on the digits in display that need to be turned on
//Then displays the digit, delaying an amount of time set in macros to improve persistence of vision
//*******************************************************************************
void display_digits(unsigned int val, char axis)
{
    turn_off_led_digits();

    unsigned int digit = 0;
    unsigned int number = val;
    volatile unsigned int i;

    if (val >= 0 && val <= 9)
    {
        //If the number is less than 10, turn on first digit and display the value
        digits_on = 1 << 0;
        digit = number % 10;
        P2OUT = DIGIT_4;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);
        display_decimal_point(axis, digits_on);
    }
    else if (val >= 10 && val <= 99)
    {
        //If the number is more than 10, but less than 99
        //Turn on 1s place digit, display value, and delay
        //Then extract 10s place digit and repeat
        digits_on = (1 << 0) + (1 << 1);
        digit = number % 10;
        P2OUT = DIGIT_4;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);
        digit = (number / 10) % 10;
        P2OUT = DIGIT_3;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);
        if (axis == 'z')
        {
            display_decimal_point('z', digits_on);
        }
        else if (axis == 'y')
        {
            display_decimal_point('y', digits_on);
        }
        else if (axis == 'x')
        {
            display_decimal_point('x', digits_on);
        }

    }
    else if (val >= 100 && val <= 999)
    {
        //If the number is more than 10, but less than 99
        //Turn on 1s place digit, display value, and delay
        //Then extract 10s place digit and repeat
        //Then extract 1000s place digit and repeat again

        digits_on = (1 << 0) + (1 << 1) + (1 << 2);
        digit = number % 10;
        P2OUT = DIGIT_4;
        digits_on |= 1 << 0;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);
        digit = (number / 10) % 10;
        P2OUT = DIGIT_3;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);

        if (axis == 'z')
        {
            display_decimal_point('z', digits_on);
        }
        _delay_cycles(DIGDELAY);
        digit = (number / 100) % 10;
        P2OUT = DIGIT_2;
        led_display_num(digit);
        if (axis == 'y')
        {
            display_decimal_point('y', digits_on);
        }
        else if (axis == 'x')
        {
            display_decimal_point('x', digits_on);
//            P2OUT = DIGIT_1;
//            turn_off_led_segments();
//            lit_led_segment(DP);
        }

    }
    else if (val >= 1000 && val <= 9999)
    {
        //Do the same process as above, but for all 4 digits.
        digits_on = (1 << 0) + (1 << 1) + (1 << 2) + (1 << 3);
        digit = number % 10;
        P2OUT = DIGIT_4;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);
        digit = (number / 10) % 10;
        P2OUT = DIGIT_3;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);

        if (axis == 'z')
        {
            display_decimal_point('z', digits_on);
        }
        digit = (number / 100) % 10;
        P2OUT = DIGIT_2;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);

        if (axis == 'y')
        {
            display_decimal_point('y', digits_on);
        }
        digit = (number / 1000) % 10;
        P2OUT = DIGIT_1;
        led_display_num(digit);
        _delay_cycles(DIGDELAY);

        if (axis == 'x')
        {
            display_decimal_point('x', digits_on);
        }
    }
}
