# Embedded System Ohmmeter
Used the TI-MSP430 FR4133 Launchpad microcontroller(MCU) and a custom PCB to make my own ohmmeter. Just plug in a resistor to some female connectors and have the resistance value displayed on the MCU's LCD screen. Fully auto-ranging, accurate for resistance values between 10-500k ohms.

## How it works
### Hardware
Everyone knows Ohm's law: V=IR. That's basically it.

If you use a known resistance in series with an unknown resistance, it works as a voltage divider. You can then measure the voltage drop across the unknown resitor and since you know how much voltage you are supplying through the resistors, you can calculate the voltage drop across the known resistor. From this you can calculate the current, and since the resistors are in series, the current through both resistors is the same. Now you know the voltage across the resistor and the current through it and so you have the resistance.

![](voltage_dividers.PNG)

Note in the above picture, there are diodes that have been added. This is because we don't want current to flow backwards through any of the voltage dividing channels and affect the measurement. This won't really affect the accuracy because the voltage drop across the diode can be subtracted assuming it is constant.

Now use this idea multiple times. Use different voltage dividers with different known resistance values for better accuracy across a wider range. To select the best voltage divider to use, try them all until the voltage drop across the unknown resistance is within a desired range.

For very low resistances, it's more accurate to use a constant current source. This is slightly different but still uses the same principles. Simply pass a constant current through the resistor and measure the voltage drop across it, now you know the current nd voltage, and you can calculate the resistance. Here is a diagram of the constant current source used in this design:

![](current_source.png)

Note: an op-amp buffer is used to indirectly measure the voltage across the resistor here without affecting the measurement. It is not shown in the picture but it can be seen in the full schematic.

### Software
The software is relatively simple. There are a few main steps that the program goes through:
1. Setup the MCU
2. Try each of the divider channels until the best one is found
3. If the resistance is low enough, use the current source
4. Repeat the steps 2-3 a few times and take an average of the resistance calculated
5. Display the resistance on the LCD

Here's an example of how the code works.
Trying the different voltage dividers:

```C
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
```

## Final product
![](final_product.png)

In the picture, the device is measuring a 20 ohm resistor, so it's pretty accurate .

## Limitations and improvements
This Ohmmeter doesn't quite measure resistance as high as I originally intended. I was hoping it would go up to at least 1M ohm. In my initial prototype, it could go that high, just not very accurately. In my final design, it won't display anything that high and the GPIO pins of the MCU start outputting ~4.5V when they're only supposed to output 3.3V max. So that's a problem. It might be because I didn't protect my circuit well enough when using the 5V power supply for my op-amps. For future iterations of this project, that would need to be fixed so that it can measure high resistance and also not slowly destory the MCU.
