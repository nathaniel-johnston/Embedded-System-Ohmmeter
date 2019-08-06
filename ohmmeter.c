#include "driverlib.h"
#include "Board.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

float CURRENT = 0.01;

uint8_t mempin1 [6] = {LCD_E_MEMORY_BLINKINGMEMORY_4, LCD_E_MEMORY_BLINKINGMEMORY_6,LCD_E_MEMORY_BLINKINGMEMORY_8,
                       LCD_E_MEMORY_BLINKINGMEMORY_10, LCD_E_MEMORY_BLINKINGMEMORY_2, LCD_E_MEMORY_BLINKINGMEMORY_18};
uint16_t resistorPin[5] = {GPIO_PIN5, GPIO_PIN4, GPIO_PIN3, GPIO_PIN6, GPIO_PIN7};

int charLut1 [12] = {0xFC, 0x60, 0xDB, 0xF3, 0x67, 0xB7, 0xBF, 0xE0, 0xFF, 0xF7, 0x00, 0x6C};
int others[3] = {0x72, 0xA0, 0x01};
int measurement = 0;
float resistor[5] = {5000, 50300, 200000, 553000, 1094000};

void setup() {
    // Hold watchdog
    WDT_A_hold(WDT_A_BASE);

    //Set A8 as an input pin.
    //Set appropriate module function
    GPIO_setAsPeripheralModuleFunctionInputPin(
        GPIO_PORT_ADC8,
        GPIO_PIN_ADC8 ,
        GPIO_FUNCTION_ADC8);

    GPIO_setAsPeripheralModuleFunctionInputPin(
        GPIO_PORT_P4,
        GPIO_PIN1 + GPIO_PIN2,
        GPIO_PRIMARY_MODULE_FUNCTION
        );

    //PA.x output
    GPIO_setAsOutputPin(
        GPIO_PORT_PA,
        GPIO_PIN0 + GPIO_PIN3 + GPIO_PIN4 + GPIO_PIN5
        + GPIO_PIN6 + GPIO_PIN7
        );

    GPIO_setAsOutputPin(
            GPIO_PORT_P5,
            GPIO_PIN1
            );

    //Initialize the ADC Module

    /* Base Address for the ADC Module
    * Use internal ADC bit as sample/hold signal to start conversion
    * USE MODOSC 5MHZ Digital Oscillator as clock source
    * Use default clock divider of 1*/

    ADC_init(ADC_BASE,
       ADC_SAMPLEHOLDSOURCE_SC,
       ADC_CLOCKSOURCE_ADCOSC,
       ADC_CLOCKDIVIDER_1);

    ADC_enable(ADC_BASE);

    //Set external frequency for XT1
    CS_setExternalClockSource(32768);

    //Select XT1 as the clock source for ACLK with no frequency divider
    CS_initClockSignal(CS_ACLK, CS_XT1CLK_SELECT, CS_CLOCK_DIVIDER_1);

    //Start XT1 with no time out
    CS_turnOnXT1(CS_XT1_DRIVE_0);


    /* Base Address for the ADC Module
    * Sample/hold for 16 clock cycles
    * Do not enable Multiple Sampling*/

    ADC_setupSamplingTimer(ADC_BASE,
           ADC_CYCLEHOLD_16_CYCLES,
           ADC_MULTIPLESAMPLESDISABLE);

    ADC_configureMemory(ADC_BASE,
           ADC_INPUT_A8,
           ADC_VREFPOS_AVCC,
           ADC_VREFNEG_AVSS);

    ADC_clearInterrupt(ADC_BASE,
           ADC_COMPLETED_INTERRUPT);

    //Enable Memory Buffer interrupt
    ADC_enableInterrupt(ADC_BASE,
           ADC_COMPLETED_INTERRUPT);

    PMM_unlockLPM5();

    LCD_E_setPinAsLCDFunctionEx(LCD_E_BASE, LCD_E_SEGMENT_LINE_0, LCD_E_SEGMENT_LINE_26);
    LCD_E_setPinAsLCDFunctionEx(LCD_E_BASE, LCD_E_SEGMENT_LINE_36, LCD_E_SEGMENT_LINE_39);

    LCD_E_initParam initParams = LCD_E_INIT_PARAM;
    initParams.clockDivider = LCD_E_CLOCKDIVIDER_8;
    initParams.muxRate = LCD_E_4_MUX;
    initParams.segments = LCD_E_SEGMENTS_ENABLED;

    // Init LCD as 4-mux mode
    LCD_E_init(LCD_E_BASE, &initParams);

    // LCD Operation - Mode 3, internal 3.08v, charge pump 256Hz
    LCD_E_setVLCDSource(LCD_E_BASE, LCD_E_INTERNAL_REFERENCE_VOLTAGE, LCD_E_EXTERNAL_SUPPLY_VOLTAGE);
    LCD_E_setVLCDVoltage(LCD_E_BASE, LCD_E_REFERENCE_VOLTAGE_3_08V);

    LCD_E_enableChargePump(LCD_E_BASE);
    LCD_E_setChargePumpFreq(LCD_E_BASE, LCD_E_CHARGEPUMP_FREQ_16);

    // Clear LCD memory
    LCD_E_clearAllMemory(LCD_E_BASE);

    // Configure COMs and SEGs
    // L0, L1, L2, L3: COM pins
    // L0 = COM0, L1 = COM1, L2 = COM2, L3 = COM3
    LCD_E_setPinAsCOM(LCD_E_BASE, LCD_E_SEGMENT_LINE_0, LCD_E_MEMORY_COM0);
    LCD_E_setPinAsCOM(LCD_E_BASE, LCD_E_SEGMENT_LINE_1, LCD_E_MEMORY_COM1);
    LCD_E_setPinAsCOM(LCD_E_BASE, LCD_E_SEGMENT_LINE_2, LCD_E_MEMORY_COM2);
    LCD_E_setPinAsCOM(LCD_E_BASE, LCD_E_SEGMENT_LINE_3, LCD_E_MEMORY_COM3);

    // Make sure the used pins start LOW
    GPIO_setOutputLowOnPin(
        GPIO_PORT_PA,
        GPIO_PIN0 + GPIO_PIN3 + GPIO_PIN4 + GPIO_PIN5
        + GPIO_PIN6 + GPIO_PIN7
        );

    GPIO_setOutputLowOnPin(
               GPIO_PORT_P5,
               GPIO_PIN1
               );
}

void displayChar(int charLutIndex, int digitIndex) {
    //12 = decimal point
    if(charLutIndex != 12) {
        LCD_E_setMemory(LCD_E_BASE, mempin1[digitIndex], charLut1[charLutIndex]);
    }

    // >9 for k or M
    if(charLutIndex > 9) {
        LCD_E_setMemory(LCD_E_BASE, mempin1[digitIndex] + 1, others[charLutIndex - 10]);
    }
}

void clearLcd() {
    int i;

    for (i = 0; i < 6; i++) {
        LCD_E_setMemory(LCD_E_BASE, mempin1[i], 0x00);
        LCD_E_setMemory(LCD_E_BASE, mempin1[i] + 1, 0x00);
    }
}

void displayResistance(double resistance) {
    unsigned int intVal = 0;
    int setting = 0;
    int length = 0;
    int i = 0;

    clearLcd();

    //Display as kOhm or MOhm
    if(resistance > 1000000) {
        return;
        resistance = resistance/1000000;
        setting = 2;
    }
    else if (resistance > 1000) {
        resistance = resistance/1000;
        setting = 1;
    }

    //Perform some math to display the characters on the screen
    intVal = 100*resistance;
    length = log10(intVal) + 1;

    while (intVal > 0) {
        displayChar(intVal % 10, length - i - 1);
        intVal = intVal / 10;
        i++;
    }

    //Display the decimal point
    displayChar(12, i-3);

    //display M
    if (setting  == 2) {
        displayChar(11, length);
    }
    //display k
    else if (setting == 1) {
        displayChar(10, length);
    }
}

float useCurrentSource() {
    GPIO_setOutputHighOnPin(
        GPIO_PORT_P5,
        GPIO_PIN1
        );

    ADC_startConversion(ADC_BASE,
                   ADC_SINGLECHANNEL);

    //LPM0, ADC10_ISR will force exit
    __bis_SR_register(CPUOFF + GIE);

    GPIO_setOutputLowOnPin(
            GPIO_PORT_P5,
            GPIO_PIN1
            );

    return ((float)measurement/1023*3.3 - 0.33) / CURRENT;
}

float pollChannels() {
    float voltage = 5;
    float resistance;
    int i = -1;

    //Try all the voltage channels until the best one is found
    while (voltage > 1.65 && i < 5) {
        i++;

        GPIO_setOutputHighOnPin(
            GPIO_PORT_PA,
            resistorPin[i]
            );

        // Take a measurement from the ADC
        ADC_startConversion(ADC_BASE,
               ADC_SINGLECHANNEL);

       //LPM0, ADC10_ISR will force exit
       __bis_SR_register(CPUOFF + GIE);


       GPIO_setOutputLowOnPin(
                   GPIO_PORT_PA,
                   resistorPin[i]
                   );

       voltage = (float)measurement/1023*3.3;
    }

    // Calculate the current through the unknown resistor by calculating it through the known resistor
    float current = (3.3 - 0.545 - voltage)/resistor[i];
    //Calculate the resistance through Ohm's law
    resistance = voltage / current;

    //You can use the current source for low resistor values
    if (i == 0 && resistance <= 280) {
        return useCurrentSource();
    }

    return resistance;
}

void main(void)
{
    double resistance;
    double avg = 0.0;

    setup();

    LCD_E_on(LCD_E_BASE);

    while (1) {

        resistance = pollChannels();

        if (resistance > avg + avg*0.05 || resistance < avg - avg*0.05) {
            avg = resistance;
            int i = 0;

            // take an average measurement
            for (i = 0; i < 4; i++) {
                resistance = pollChannels();

                avg = avg + resistance;
            }

            avg = avg/5.0;


            displayResistance(avg);
        }
    }

}


//ADC10 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=ADC_VECTOR
__interrupt
#elif defined(__GNUC__)
__attribute__((interrupt(ADC_VECTOR)))
#endif
void ADC_ISR (void)
{
    switch (__even_in_range(ADCIV,12)){
        case  0: break; //No interrupt
        case  2: break; //conversion result overflow
        case  4: break; //conversion time overflow
        case  6: break; //ADC10HI
        case  8: break; //ADC10LO
        case 10: break; //ADC10IN
        case 12:{       //ADC10IFG0

        measurement = ADC_getResults(ADC_BASE);

          //Clear CPUOFF bit from 0(SR)
            //Breakpoint here and watch ADC_Result
          __bic_SR_register_on_exit(CPUOFF);
          break;
        }
        default: break;
    }
}


