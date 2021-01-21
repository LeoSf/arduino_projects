/**
 * @file esp32_poc_interrupt_timer.ino
 *
 * @brief Proof of Concept: timer interrupt on ESP32
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
 *      This program test a timer interrupt.
 *
 */

int interval_s = 10;

volatile int interruptCounter;
int totalInterruptCounter;

hw_timer_t * timer = NULL;

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer() {
    portENTER_CRITICAL_ISR(&timerMux);
    interruptCounter++;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void setup() {

    Serial.begin(115200);

    /* with a flash freq of 80 MHZ - timer configured to count in [us] */
    timer = timerBegin(0, 80, true);
    /* timerBegin(0, 80, true);
            0: hw timer {0, 1, 2, 3}
            80: prescaler
            true: flag -  count up (true) or down (false).
    */

    timerAttachInterrupt(timer, &onTimer, true);
    /* timerAttachInterrupt(timer, &onTimer, true);
            timer: pointer to the initialized timer
            onTimer: address of the function that will handle the interrupt
            true: if the interrupt to be generated is edge (true) or level (false)
    */

    /* set counter value */
    timerAlarmWrite(timer, interval_s*1000000, true);          // set timer to 1 [s]
    /* true: flag indicating if the timer should automatically reload upon generating the interrupt. */

    timerAlarmEnable(timer);

}

void loop() {

    if (interruptCounter > 0) {

        portENTER_CRITICAL(&timerMux);
        interruptCounter--;
        portEXIT_CRITICAL(&timerMux);

        totalInterruptCounter++;

        Serial.print("An interrupt as occurred. Total number: ");
        Serial.println(totalInterruptCounter);

    }
}



/* Some additional notes:

https://techtutorialsx.com/2017/10/07/esp32-arduino-timer-interrupts/

The alarms
The ESP32 has two timer groups, each one with two general purpose hardware timers. All
the timers are based on 64 bits counters and 16 bit prescalers [1].

The prescaler is used to divide the frequency of the base signal (usually 80 MHz), which
is then used to increment / decrement the timer counter [2]. Since the prescaler has 16
bits, it can divide the clock signal frequency by a factor from 2 to 65536 [2], giving a
lot of configuration freedom.

The timer counters can be configured to count up or down and support automatic reload
and software reload [2]. They can also generate alarms when they reach a specific value,
defined by the software [2]. The value of the counter can be read by the software
program [2].


Setup function
* typically the frequency of the base signal used by the ESP32 counters is 80 MHz
* if we divide this value by 80 (using 80 as the prescaler value), we will get a signal
with a 1 MHz
* From the previous value, if we invert it, we know that the counter will be incremented
at each microsecond


----
interrupt handling routines should only call functions also placed in IRAM, as can be
seen here in the IDF documentation:
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/general-notes.html?highlight=iram



*/
