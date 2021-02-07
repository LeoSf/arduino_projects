/**
* @file digi_test_builtin_led.ino
*
* @brief Brief description of the file
*
* @ingroup noPackageYet
* (Note: this needs exactly one @defgroup somewhere)
*
* @author Leo Medus
*
* Details:
*  [setup] - DigiSpark rev. 3
*          - The circuit: No external hardware needed
*
* Date: 07/02/2020 (dd/mm/yyyy)
*
*      definir modificaciones
*
*
*
*
*/



const int ledPin = 1;

void setup()
{
  pinMode(ledPin, OUTPUT);
}

void loop()
{
  digitalWrite(ledPin, HIGH);
  delay(1000);
  digitalWrite(ledPin, LOW);
  delay(1000);
}
