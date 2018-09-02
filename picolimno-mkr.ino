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

#define DEBUG_BUILD true
// #define DEBUG_BUILD false

#define DEBUG(x) if (DEBUG_BUILD) { Serial.print(x); /* Serial1.print(x); */ }
#define DEBUGLN(x) if (DEBUG_BUILD) { Serial.println(x); /* Serial1.print(x); */ }

// #ifdef DEBUG_BUILD
// #  define DEBUG(x) do { Serial.print(x); /* Serial1.print(x); */ } while(0)
// #  define DEBUGLN(x) do { Serial.println(x); /* Serial1.println(x); */ } while(0)
// #else
// #  define DEBUG(x) do {} while (0)
// #  define DEBUGLN(x) do {} while (0)
// #endif

#define PRINT(x) if (true) { Serial.print(x); /* Serial1.print(x); */ }
#define PRINTLN(x) if (true) { Serial.println(x); /* Serial1.print(x); */ }

#include "secrets.h"
/** 
    #define PIN_CODE
    
    #define APN_NAME
    #define APN_USERNAME
    #define APN_PASSWORD
*/

#include "App.h"

App& app = App::getInstance(F(APN_NAME), F(APN_USERNAME), F(APN_PASSWORD));

void setup() {
  Serial.begin(115200);
//  Serial1.begin(57600);
//  while (!Serial) ;
  delay(5000);

  PRINTLN(F(__FILE__));
  PRINT(F("Compiled on ")); PRINTLN(F(__DATE__));
  PRINT(F(" at ")); PRINTLN(F(__TIME__));
  PRINT(F(" for ")); PRINTLN(F(USB_PRODUCT));

  if (!app.setup()) {
    PRINTLN(F("Error in App::Setup!"));
    exit(0);
  }
}

void loop() {
  if (!app.loop()) {
    PRINTLN(F("Error in App::Loop!"));
    exit(0);
  }
}
 
