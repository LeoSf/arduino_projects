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
 * https://randomnerdtutorials.com/install-esp32-filesystem-uploader-arduino-ide/
 */

#include "SPIFFS.h"

void setup() {
    Serial.begin(115200);

    if(!SPIFFS.begin(true)){
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    File file = SPIFFS.open("/test_example.txt");
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.println("File Content:");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

void loop() {

}
