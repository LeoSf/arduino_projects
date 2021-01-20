/**
 * @file esp32_poc_blink.ino
 *
 * @brief Proof of conecept to test the board
 *
 * @ingroup noPackageYet
 * (Note: this needs exactly one @defgroup somewhere)
 *
 * @author Leo Medus
 *
 * Details:
 *  [setup] - ESP-32 Dev Kit C V2
            - led and 1k resisitor connected to pin 2.
 *
 *      This code blinks a led conected to pin 2.
 *               -------
 *
 * Date: 19/01/2020 (dd/mm/yyyy)
 *
 * The ESP32 LED PWM (Pulse width modulation) controller has 16 independent
 * channels that can be configured to generate PWM signals with different
 * properties. All pins that can act as outputs can be used as PWM pins
 * (GPIOs 34 to 39 cannot generate PWM)
 */

const int ledPin = 2;            // ledPin refers to ESP32 GPIO 2

 // the setup function runs once when you press reset or power the board
void setup() {
    pinMode(ledPin, OUTPUT);      // initialize digital pin ledPin as an output.
}

// the loop function runs over and over again forever
void loop() {
    digitalWrite(ledPin, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(1000);                  // wait for a second

    digitalWrite(ledPin, LOW);    // turn the LED off by making the voltage LOW
    delay(1000);                  // wait for a second
}
