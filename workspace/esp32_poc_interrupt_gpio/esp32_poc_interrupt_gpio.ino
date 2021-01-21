/**
 * @file esp32_poc_interrupt.ino
 *
 * @brief Proof of Concept: GPIO interrupt on ESP32
 *
 * @ingroup noPackageYet
 * (Note: this needs exactly one @defgroup somewhere)
 *
 * @author Leo Medus
 *
 * Details:
 *  [setup] - ESP-32 Dev Kit C V2
 *          - The circuit: No external hardware needed
 *
 * Date: 21/01/2020 (dd/mm/yyyy)
 *
 *      This program test a GPIO interrupt.
 *
 * attachInterrupt(digitalPinToInterrupt(GPIO), function, mode);
 *
 * The third argument is the mode. There are 5 different modes:
 * LOW: to trigger the interrupt whenever the pin is LOW;
 * HIGH: to trigger the interrupt whenever the pin is HIGH;
 * CHANGE: to trigger the interrupt whenever the pin changes value – for example from HIGH to LOW or LOW to HIGH;
 * FALLING: for when the pin goes from HIGH to LOW;
 * RISING: to trigger when the pin goes from LOW to HIGH.
 *
 * millis() can return the number of milliseconds that have passed since the program first started.
 * Note: IRAM_ATTR is used to run the interrupt code in RAM, otherwise code is stored in flash and it’s slower.
 */

const int pinGPIO_interrupt = 27;
const int pinGPIO_led = 27;

const long interval_ms = 1000;      // interval at which to blink (milliseconds)

int ledState = LOW;                 // ledState used to set the LED
boolean enable_timer = false;
unsigned long prev_time_ms = 0;     // will store last time LED was updated


void IRAM_ATTR isr_gpioPin()
{
    Serial.println("[Interrupt service routine called]");
    digitalWrite(pinGPIO_led, HIGH);
    enable_timer = true;
    prev_time_ms = millis();
}

void setup() {

    pinMode(pinGPIO_led, OUTPUT);

    attachInterrupt(digitalPinToInterrupt(pinGPIO_interrupt), isr_gpioPin, RISING);

    // prints title with ending line break
    Serial.println("[ PoC: ISR ]");
}

void loop() {

    unsigned long curr_time_ms = millis();

    if (curr_time_ms - prev_time_ms >= interval_ms)
    {
        // save the last time you blinked the LED
        prev_time_ms = curr_time_ms;

        // if the LED is off turn it on and vice-versa:
        if (ledState == LOW)
        {
            ledState = HIGH;
        } else
        {
            ledState = LOW;
        }

        // set the LED with the ledState of the variable:
        digitalWrite(pinGPIO_led, ledState);
    }

}
