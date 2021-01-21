/**
 * @file esp32_poc_pwm.ino
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
 *          - led and 330 Ohm resisitor connected to pin 2.
 *
 *      This code uses pwm to control brightness over a led connected on pin 2.
 *                     ---            ----------
 *
 * Date: 20/01/2020 (dd/mm/yyyy)
 */


#define LEDC_CHANNEL_0      0
#define LEDC_TIMER_13_BIT   13
#define LEDC_BASE_FREQ      5000
#define LED_PIN             2

int brightness = 0;
int fadeAmount = 5;

void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255)
{
    uint32_t duty = (8191 / valueMax) * min(value, valueMax);
    ledcWrite(channel, duty);
}

void setup() {
    ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
    ledcAttachPin(LED_PIN, LEDC_CHANNEL_0);
}

void loop() {
    ledcAnalogWrite(LEDC_CHANNEL_0, brightness);

    brightness = brightness + fadeAmount;

    if (brightness <= 0 || brightness >= 255) {
        fadeAmount = -fadeAmount;
    }

    delay(30);
}
