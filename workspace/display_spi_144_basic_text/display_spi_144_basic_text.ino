/**
 * @file display_spi_144_basic_text.ino
 *
 * @brief Basic source code
 *
 * @ingroup noPackageYet
 * (Note: this needs exactly one @defgroup somewhere)
 *
 * @author Leo Medus
 * Contact: leomedus@gmail.com
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
/*
Teensy 3.x can use: 2,6,9,10,15,20,21,22,23
Arduino's 8 bit: any
DUE: check arduino site
If you do not use reset, tie it to +3V3
*/


TFT_ILI9163C display = TFT_ILI9163C(__CS, __DC);

float p = 3.1415926;

float humidity_val = 12.57;

void setup(void) {
  display.begin();


  uint16_t time = millis();
  time = millis() - time;

//  lcdTestPattern();
//  delay(1000);

  display.clearScreen();
  display.setCursor(0,0);
  delay(1000);

  // tftPrintTest();

  display_basic_info();

  delay(2000);


}

void loop()
{
    /* empty void loop */
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
    display.setTextColor(BLUE);
    display.setTextSize(3);
    display.print(humidity_val);

}

void test_text()
{
    display.clearScreen();
    display.setCursor(0, 2);

    /* text in red */
    display.setTextColor(RED);
    display.setTextSize(1);
    display.println("Hello World!");

    /* text in yellow, background in green */
    display.setTextColor(YELLOW, GREEN);
    display.setTextSize(2);
    display.print("Hello Wo\n");


    display.setTextColor(BLUE);
    display.setTextSize(3);
    display.print(12.57);
}
