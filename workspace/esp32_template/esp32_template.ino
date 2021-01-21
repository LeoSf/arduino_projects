/**
 * @file esp32_template.ino
 *
 * @brief Brief description of the file
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
 *      This program receives a byte from the serial port and displays it again (echo).
 *
 */
int inByte = 33;

void setup() {
    //Initialize serial and wait for port to open:
    Serial.begin(9600);

    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB port only
    }

    // prints title with ending line break
    Serial.println("[ Template serial conection ]");

    waiting_connection();
}

void loop() {

    if (Serial.available()) {      // If anything comes in Serial (USB),
        inByte = Serial.read();
        // Serial1.write(Serial.read());   // read it and send it out Serial1 (pins 0 & 1)

        Serial.print(inByte);
        Serial.print(" ");
        Serial.print(inByte, DEC);
        Serial.print(" ");
        Serial.print(inByte, HEX);
        Serial.print(" ");
        Serial.println(inByte, BIN);
    }

    delay(1000);        /* 1 sec delay in the loop */

}


void waiting_connection() {
    Serial.println("waiting input data from serial port");   // send an initial string
    while (Serial.available() <= 0) {
        Serial.print(".");
        delay(1000);
    }
}
