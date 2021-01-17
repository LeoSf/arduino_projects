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
 * Notes:
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

/*
Teensy3.x and Arduino's
You are using 4 wire SPI here, so:
 MOSI:  11//Teensy3.x/Arduino UNO (for MEGA/DUE refere to arduino site)
 MISO:  12//Teensy3.x/Arduino UNO (for MEGA/DUE refere to arduino site)
 SCK:   13//Teensy3.x/Arduino UNO (for MEGA/DUE refere to arduino site)
 the rest of pin below:
 */
#define __CS 10
#define __DC 9

TFT_ILI9163C display = TFT_ILI9163C(__CS, __DC);

float humidity_val = 12.57;


const int analogInPin = A0;  // Analog input pin that the potentiometer is attached to

int sensorValue = 0;        // value read from the pot
int outputValue = 0;        // value output to the PWM (analog out)


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

void read_humidity()
{
    // read the analog in value:
    sensorValue = analogRead(analogInPin);
    // map it to the range of the analog out:
    outputValue = map(sensorValue, 0, 1023, 0, 100);

    // change the analog out value:
    // analogWrite(analogOutPin, outputValue);

    // print the results to the Serial Monitor:
    Serial.print("sensor = ");
    Serial.print(sensorValue);
    Serial.print("\t output = ");
    Serial.println(outputValue);

    // wait 2 milliseconds before the next loop for the analog-to-digital
    // converter to settle after the last reading:
    // delay(2);
}

void display_basic_info()
{
    display.clearScreen();

    /* text in yellow, background in green */
    display.setCursor(0, 5);
    display.setTextColor(WHITE, GREEN);
    display.setTextSize(2);
    display.print("  Sensor\nde humedad");
    display.print("\n\n");

    // display.setCursor(0, 20);
    display.setTextColor(WHITE, GREEN);
    display.setTextSize(2);
    display.print(" Plantita\n");
    display.print(" : baby :");
    display.print("\n\n");

    // display.setCursor(0, 60);
    // display.setTextColor(BLUE);
    // display.setTextSize(3);
    // display.print(sensorValue);

}
void display_sensor_value()
{
    display.fillRect(48, 98, 55, 25, GREEN);

    display.setCursor(50, 100);
    display.setTextColor(BLUE);
    display.setTextSize(3);
    display.print(sensorValue);

}
