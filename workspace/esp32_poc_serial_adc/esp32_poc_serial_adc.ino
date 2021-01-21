/**
 * @file esp32_poc_serial_adc.ino
 *
 * @brief Read two analog values and control one GPIO from serial communication.
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
 */

#define ADC_UPPER_LIMIT 4095
#define OFF 0
#define ON 1


const int pinSensor0 = 2;
const int pinSensor1 = 4;
const int pinGPIO = 5;

int firstSensor = 0;    // first analog sensor
int secondSensor = 0;   // second analog sensor
int inByte = 0;         // incoming serial byte

char *gpioStatus[] = {"OFF", "ON"};
int gpioValue;

float firstSensor_mapped;
float secondSensor_mapped;

void setup() {
    // start serial port at 9600 bps and wait for port to open:
    Serial.begin(115200);
    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB port only
    }

    pinMode(pinGPIO, OUTPUT);   // digital sensor is on digital pin 2

    Serial.println("PoC: serial comunication [ESP32]");
    Serial.println("--------------------------------");
    Serial.println("Send 'a' to turn on");
    Serial.println("Send 's' to turn off");

    waiting_connection();  // send a byte to establish contact until receiver responds
}

void loop() {
    // if we get a valid byte, read analog ins:
    if (Serial.available() > 0) {
        // get incoming byte:
        inByte = Serial.read();

        // read first analog input:
        firstSensor = analogRead(pinSensor0);
        firstSensor_mapped = map(firstSensor, 0, ADC_UPPER_LIMIT, 0, 100);
        // read second analog input:
        secondSensor = analogRead(pinSensor1);
        secondSensor_mapped = map(secondSensor, 0, ADC_UPPER_LIMIT, 0, 100);

        // // read switch, map it to 0 or 255
        // gpioStatus = map(digitalRead(2), 0, 1, 0, 255);

        if(inByte == 'a')
        {
            digitalWrite(pinGPIO, HIGH);
            gpioValue = ON;
        }
        else if(inByte == 's')
        {
            digitalWrite(pinGPIO, LOW);
            gpioValue = OFF;
        }

        // send sensor values:
        Serial.print("Sensor values: ");
        Serial.print(firstSensor_mapped);
        Serial.print("% ,");
        Serial.print(secondSensor_mapped);
        Serial.print("% , ");
        Serial.println(gpioStatus[gpioValue]);
    }
}

void waiting_connection() {
    Serial.print("waiting input data from serial port");   // send an initial string
    while (Serial.available() <= 0) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println();
}
