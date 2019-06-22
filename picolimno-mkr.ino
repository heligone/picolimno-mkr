/*
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

 /**
  * @file
    Picolimno MKR V1.0 project
    picolimno-mkr.ino
    Purpose: Main program calling App Class

    @author M. SIBERT
    @version 1.0 25/02/2018
*/
  Uart mySerial (&sercom3, 0, 1, SERCOM_RX_PAD_1, UART_TX_PAD_0); // Create the new UART instance assigning 

#define DEBUG_BUILD 1

#ifdef DEBUG_BUILD
#  define DEBUG(x) do { Serial.print(x);mySerial.print(x); } while(0)
#else
#  define DEBUG(x) do {} while (0)
#endif

#include "secrets.h"
/** 
    #define PIN_CODE
    
    #define APN_NAME
    #define APN_USERNAME
    #define APN_PASSWORD
*/

#include "App.h"

// for adding serial port ?
#include "wiring_private.h"

App& app = App::getInstance(F(APN_NAME), F(APN_USERNAME), F(APN_PASSWORD));

void setup() {



  pinPeripheral(0, PIO_SERCOM); //Assign RX function to pin 0
  pinPeripheral(1, PIO_SERCOM); //Assign TX function to pin 1
  // BT Serial
  mySerial.begin(57600);
  // USB Serial
  Serial.begin(57600);
  // GSM Serial
  Serial1.begin(57600);
//  while (!Serial) ;
  delay(5000);

  DEBUG(F(__FILE__)); DEBUG(F("\n"));
  DEBUG(F("Compiled on ")); DEBUG(F(__DATE__)); DEBUG(F("\n"));
  DEBUG(F(" at ")); DEBUG(F(__TIME__)); DEBUG(F("\n"));
  DEBUG(F(" for ")); DEBUG(F(USB_PRODUCT)); DEBUG(F("\n"));

  if (!app.setup()) {
    DEBUG(F("Error in App::Setup!")); DEBUG(F("\n"));
    exit(0);
  }
}

void loop() {
  if (!app.loop()) {
    DEBUG(F("Error in App::Loop!")); DEBUG(F("\n"));
    exit(0);
  }
}

  void SERCOM3_Handler()
{
  mySerial.IrqHandler();
}
 
