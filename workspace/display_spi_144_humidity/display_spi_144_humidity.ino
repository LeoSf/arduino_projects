/**
 * @file display_spi_144_humidity.ino
 *
 * @brief Basic source code
 *
 * @ingroup noPackageYet
 * (Note: this needs exactly one @defgroup somewhere)
 *
 * @author Leo Medus
 *
 * Details:
 *  [setup] - arduino pro mini ATmega328P (5V @16MHz)
 *          - 1.44" 128x 128 TFT LCD with SPI Interface
 *          - Soil moisture sensor hygrometer module V1.2 capacitive for Arduino
 *
 *      This code measures the moisture present in the ground of a plant.
 *
 *
 * Notes: (values need to be updated)
 *      Dry: (520 430]
 *      Wet: (430 350]
 *      Water: (350 260]
 *
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>

// Color definitions
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#define BACKGROUND WHITE

/*
Arduino's pins
You are using 4 wire SPI here, so:
 MOSI:  11
 MISO:  12
 SCK:   13
 the rest of pin below:
 */
#define __CS 10
#define __DC 9 

TFT_ILI9163C display = TFT_ILI9163C(__CS, __DC);

/* calibration values of the sensor */
float soil_limit_dry = 820;     /* sensor value in the air */
float soil_limit_moist = 425;   /* sensor value in water */

const int analogInPin = A0;     /* Analog input pin that the potentiometer is attached to */

int sensor_value = 0;           /* raw sensor value */
float humidity_val = 0.0;       /* % relative humidity */

void setup(void) {

    // initialization of the  serial communications at 9600 bps:
    Serial.begin(9600);

    // display initialization
    display.begin();

    display.clearScreen();
    delay(1000);

    display_basic_info();

    delay(2000);

}

void loop()
{
    /* reading humidity sensor */
    read_humidity();
    display_sensor_value();
    delay(1000);
}

void check_limits()
{
    if (sensor_value < soil_limit_moist)
        soil_limit_moist = sensor_value;
    else if(sensor_value > soil_limit_dry)
        soil_limit_dry = sensor_value;
}

void read_humidity()
{
    // read the analog in value:
    sensor_value = analogRead(analogInPin);

    check_limits();
    // map it to the range of the humidity:
    // humidity_val = map(sensor_value, 0, 1023, 0, 100);
    humidity_val = (soil_limit_dry - sensor_value) / (soil_limit_dry - soil_limit_moist) * 100;

    // print the results to the Serial Monitor:
    Serial.print("sensor = ");
    Serial.print(sensor_value);
    Serial.print("\t humidity = ");
    Serial.println(humidity_val);

    // wait 2 milliseconds before the next loop for the analog-to-digital
    // converter to settle after the last reading:
    // delay(2);
}

void display_basic_info()
{
    display.clearScreen();
    display.fillRect(0, 0, 144, 144, BACKGROUND);

    /* text in yellow, background in green */
    display.fillRect(0, 4, 144, 40, GREEN);
    display.setCursor(0, 5);
    display.setTextColor(BLUE);
    display.setTextSize(2);
    display.print("  Sensor\nde humedad");
    display.print("\n\n");

    // display.setCursor(0, 20);
    display.fillRect(0, 50, 144, 40, GREEN);
    display.setTextColor(BLUE);
    display.setTextSize(2);
    display.print(" Plantita\n");
    display.print(" : baby :");
    display.print("\n\n");

}
void display_sensor_value()
{
    display.fillRect(48, 98, 55, 25, BACKGROUND);

    display.setCursor(50, 100);
    display.setTextColor(MAGENTA);
    display.setTextSize(3);
    display.print(outputValue);

}
