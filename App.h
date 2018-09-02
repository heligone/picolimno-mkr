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
 *  @file
 *  Picolimno MKR V1.0 project
 *  App.h
 *  Purpose: Contain the main application's Class.
 *
 *  @author Marc Sibert
 *  @version 1.0 25/02 - 11/04/2018
 *  @Copyright 2018 Marc Sibert
 */
#pragma once


#include <RTCZero.h>
#include <ctime>
#include <cassert>

/// @warning
/// ATTENTION FONCTIONNE SEULEMENT AVEC LA v5.13.2 (Ne pas upgrader vers la v6)
#include <ArduinoJson.h>

#include "sensors.h"
#include "alert.h"
#include "communication.h"

/// Temps en minutes entre deux mesures de distance.
#define INTERVAL_MESURES (1)

/// Temps en seconde entre deux transmissions.
#define INTERVAL_TRANSMISSION (2*60)

/// Nombre d'échantillons matériels nécessaires pour faire un échantillon brut après médiane (minimum sinon l'échantillon est invalide).
#define RANGE_SEQ_MIN 20
/// Nombre maximum de mesures matérielles pou obtenir le nombre d'échantillons matériels nécessaires.
#define RANGE_SEQ_MAX 60

// Indique la méthode de transmission :
// Si PETITES_TRAMES est defini : chaque variable est transmise séparément ;
// Sinon : toutes les variables sont transmises dans une unique requête (array JSON).
// #define PETITES_TRAMES

/**
 * Classe principale qui implémente l'application.
 */
class App {
  struct sample_t;

public:
/**
 * Factory for singleton App.
 * 
 * @return the only one (singleton) App instance.
 */
  static App& getInstance(const __FlashStringHelper apn[], const __FlashStringHelper login[], const __FlashStringHelper password[]) {
    if (!pApp) {
      pApp = new App(apn, login, password);
    }
    return *pApp;
  }
  
/**
 * Lazy setup of all devices & others parameters, especialy connections to GSM & GPRS services.
 * 
 * @return boolean value to indicate if execution was right or not. A faulty exec. means the program can't continue and should be aborted.
 */
  bool setup() {
    DEBUG(F("Configuration\n-------------\n"));
    DEBUG(F("- Mesures toutes les ")); DEBUG(INTERVAL_MESURES); DEBUG(F("s ;\n"));
    DEBUG(F("- Transmissions toutes les ")); DEBUG(INTERVAL_TRANSMISSION); DEBUG(F("s ;\n"));
    DEBUG(F("- Nombre d'echantillons matériels par mesure ")); DEBUG(RANGE_SEQ_MIN); DEBUG(F(" pour ")); DEBUG(RANGE_SEQ_MAX); DEBUG(F(" tentatives ;\n"));
#ifdef PETITES_TRAMES
    DEBUG(F("- Transmission des valeurs par trames distinctes (PETITES).\n"));
#else
    DEBUG(F("- Transmission des valeurs regroupees par trames (GRANDES).\n"));
#endif

    DEBUG(F("-------------\n"));

    DEBUGLN(F("Watchdog setup"));
    setupWatchdog(WDT_CONFIG_PER_16K);  // about 16 sec.
    
    DEBUG(F("Communication setup\n"));
    resetWatchdog();
    communication.setup();

    imei = communication.getIMEI();
    DEBUG(F("DeviceID: GSM-")); DEBUG(imei); DEBUG('\n');

    resetWatchdog();
    rtc.begin();

// Get Parameters & datetime
    DEBUG(F("Get parameters...")); DEBUG(F("\n"));
    const bool ok = communication.getParameters(imei, rtc, alert1, alert2, startTime, stopTime, reset);
    DEBUG(ok);
    DEBUG('\n');

// Sending Status
    DEBUG(F("Sending Initial Status Starting\n"));
    if (!communication.sendStatus(rtc, F("Starting"), imei)) {
      DEBUG(F("Echec de transmission. Poursuite !\n"));
    }

// Start all sensors (init...)
    if (!sensors.begin()) {
      DEBUG(F("Erreur d'initialisation des capteurs. ABANDON !\n"));
      return false;
    }

// Mesurer la distance et initialiser les alertes
    const unsigned distance = mesurerDistance();
    if (distance > 0) {   // Pas d'alerte en cas de valeur à 0
      if (alert1.enabled()) alert1.test(distance / 10.0f);
      if (alert2.enabled()) alert2.test(distance / 10.0f);
    } else {
      DEBUG(F("Première mesure de distance invalide. Poursuite !\n"));
    }

// Define next timer's interrupt
    DEBUG(F("Start timer every min.\n"));
    rtc.setAlarmSeconds(59);
    rtc.enableAlarm(rtc.MATCH_SS);
    rtc.attachInterrupt(App::intTimer);
    App::fIntTimer = false;
    
    return true;
  }

/**
 * Loop and executing all application's logic.
 * 
 * @return boolean value to indicate if execution was right or not. A faulty exec. means the program can't continue and should be aborted.
 */
  bool loop() {
    resetWatchdog();  
//    rtc.standbyMode();
    
// Si on a détecté un changement de minute
    if (!App::fIntTimer) return true;

    const byte sec = rtc.getSeconds();
    const byte minu = rtc.getMinutes();
    const byte heure = rtc.getHours();
    const unsigned long t = sec + 60U * (minu + 60U * heure);
    App::fIntTimer = false;

    DEBUG(F("Wakeup @ ")); DEBUG(getTimestamp()); DEBUG("\n");

// Vérification de l'heure de RESET quotidien
    if ((reset >= 0) && (static_cast<unsigned>(reset) == minu + 60U * heure)) {
      NVIC_SystemReset();
    }

// Vérification de la période de veille
    if ((startTime > 0) && (heure < startTime)) return true;      // Pas encore l'heure (veille)
    if ((stopTime > 0) && (heure >= stopTime)) return true;       // Trop tard (veille)
      
// Présence d'un intervale pour déclencher une mesure de distance
    if ((t % INTERVAL_MESURES) && (t % INTERVAL_TRANSMISSION)) return true;  // pas de mesure à cette minute

// Mesure de distance
    const unsigned distance = mesurerDistance();

    if (distance > 0) {   // Pas d'alerte en cas de valeur à 0
      if (alert1.enabled() && alert1.test(distance / 10.0f)) {    // Alerte activée et dépassement de seuil (montant ou descendant)
        const Communication::sample_t sample = { rtc.getEpoch(), F("alert1"), distance / 10.0f };
        if (!communication.sendSample(sample, imei)) {
          DEBUG(F("Echec de transmission. Poursuite !\n"));
        }
      }
  
      if (alert2.enabled() && alert2.test(distance / 10.0f)) {    // Alerte activée et dépassement de seuil (montant ou descendant)
        const Communication::sample_t sample = { rtc.getEpoch(), F("alert2"), distance / 10.0f };
        if (!communication.sendSample(sample, imei)) {
          DEBUG(F("Echec de transmission. Poursuite !\n"));
        }
      }
    } else {  // Transmettre une trame d'erreur (distance invalide)
      const Communication::sample_t sample = { rtc.getEpoch(), F("invalide range"), 0};
      if (!communication.sendSample(sample, imei)) {
        DEBUG(F("Echec de transmission. Poursuite !\n"));
      }
    }
      
    if ((t % INTERVAL_TRANSMISSION) == 0) {   // C'est le moment de transmission
      Communication::sample_t samples[4];
      size_t s = 0;
      
// Transmission distance si non nulle.
      if (distance > 0) { // Ne pas transmettre de mesure invalide.
        samples[s] = { rtc.getEpoch(), F("range"), distance / 10.0f };  // Utilisation du dernier échantillon (valide).
#ifdef PETITES_TRAMES
        if (!communication.sendSample(samples[s], imei)) {
          DEBUG(F("Echec de transmission. Poursuite !\n"));
        }
#endif
        ++s;
      }

// Transmission temp & hygro si OK
      float temp, hygro = 0.0;
      if (sensors.sampleAM2302(temp, hygro)) {
        samples[s] = { rtc.getEpoch(), F("temp"), temp };
#ifdef PETITES_TRAMES
        if (!communication.sendSample(samples[s], imei)) {
          DEBUG(F("Echec de transmission. Poursuite !\n"));
        }
#endif
        ++s;
        DEBUG(F("Temperature : ")); DEBUG(temp); DEBUG('\n');
          
        samples[s] = { rtc.getEpoch(), F("hygro"), hygro };
#ifdef PETITES_TRAMES
        if (!communication.sendSample(samples[s], imei)) {
          DEBUG(F("Echec de transmission. Poursuite !\n"));
        }
#endif
        ++s;
        DEBUG(F("Hygrometrie : ")); DEBUG(hygro); DEBUG('\n');
      } else { 
        DEBUG(F("Echec de mesure de temps & hygro!\n"));
      }

// Transmission vbat
      const float vBat = sensors.sampleBattery();
      samples[s] = { rtc.getEpoch(), F("vbat"), vBat };
#ifdef PETITES_TRAMES
        if (!communication.sendSample(samples[s], imei)) {
          DEBUG(F("Echec de transmission. Poursuite !\n"));
        }
#endif
      ++s;
      DEBUG("Batterie : "); DEBUG(vBat); DEBUG('\n');

// Si grandes trames, transmission de l'ensemble
#ifndef PETITES_TRAMES
      if (!communication.sendSamples(samples, s, imei)) {
        DEBUG(F("Echec de transmission. Poursuite !\n"));
      }
#endif

// Récupération des paramètres
      DEBUG(F("Get parameters..."));
      const bool ok = communication.getParameters(imei, rtc, alert1, alert2, startTime, stopTime, reset);
      DEBUG(ok);
      DEBUG('\n');
    }  
    return true;
  }

protected:
/**
 * Constructor initializing all global parameters for the following App methodes, setup() & loop().
 * 
 * @param apn APN name as PROGMEM char*
 * @param login APN login as PROGMEM char*
 * @param password APN login's password as PROGMEM char*
 */
  App(const __FlashStringHelper apn[], const __FlashStringHelper login[], const __FlashStringHelper password[]) :
    sensors(TRIGGER, ECHO, AM2302),       ///< Initialisation de capteurs (broches de connexion)
    communication(Communication::getInstance(apn, login, password, F("api.picolimno.fr"), 80)),  ///< Initialisation de la communication.
    alert1(),                             ///< Initialisation de l'alerte Rouge (seuil et hystérésis)
    alert2(),                             ///< Initialisation de l'alerte Orange  (seuil et hystérésis)
    startTime(0),                         ///< Heure de démarrage des mesures (HH) 
    stopTime(0),                          ///< Heure de fin des mesures (HH)
    reset(-1)
  {
  }

/**
 * Called every Timer's interruption.
 * Wake the CPU up.
 */
  static 
  void intTimer() {
    App::fIntTimer = true;
  }
  
  
/**
 * Retourne la date et l'heure maintenues par la RTC locale.
 * 
 * @return L'heure actuelle au format ISO3339 (https://www.ietf.org/rfc/rfc3339.txt).
 */
  String getTimestamp() const {
    char buffer[25];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ", 2000 + rtc.getYear(), rtc.getMonth(), rtc.getDay(), rtc.getHours(), rtc.getMinutes(), rtc.getSeconds() );
    return String(buffer);
  }

/**
 * Trie directement le tableau d'entiers non-signés afin d'obtenir la médiane en position centrale du tableau.
 * @warning Le tableau ne sera pas entièrement trié !
 * 
 * @A Un tableau d'entiers non-signer qui verra sa médiane placée en son centre.
 * @n Position du pivot, par défaut y mettre l'indice du milieu du tableau.
 */
  void insertionSortR(unsigned A[], const int n) const {
     if (n > 0) {
        insertionSortR(A, n-1);
        const unsigned x = A[n];
        int j = n - 1;
        while (j >= 0 and A[j] > x) {
            A[j+1] = A[j];
            --j;
        }
        A[j+1] = x;
     }
  }

/**
 * 
 */
  unsigned mesurerDistance() const {
    unsigned d[RANGE_SEQ_MIN];
    unsigned n = 0; // nb échantillons valides
    for (unsigned i = 0; i < RANGE_SEQ_MAX; ++i) {
      const unsigned s = sensors.sampleRange();
      if (s > 0) {
        d[n++] = s;
        DEBUG(s); DEBUG(F("--"));
        if (n >= RANGE_SEQ_MIN) break; // Objectif atteint
      }
    }
    DEBUG('\n');
      
    if (n > 1) {
      qsort(d, n, sizeof(unsigned), [](const void* a, const void* b) -> int { 
        const unsigned int_a = * ( (unsigned*) a );
        const unsigned int_b = * ( (unsigned*) b );
        return (int_a > int_b) - (int_a < int_b);
      });
    }

    const unsigned distance = (n >= RANGE_SEQ_MIN) ? d[n / 2] : 0;            
    DEBUG(F("Distance : ")); DEBUG(distance / 10.0f); DEBUG(F(" - Ech. : ")); DEBUG(n); DEBUG('\n');
    return distance;
  }

/**
 * Set up the generic clock (GCLK2) used to clock the watchdog timer at 1.024kHz.
 * 
 * @param aConfig divider, default WDT_CONFIG_PER_1K (about 1 sec.)
 */
  void setupWatchdog(const unsigned aConfig = WDT_CONFIG_PER_1K) const {
 // 
    REG_GCLK_GENDIV = GCLK_GENDIV_DIV(4) |            // Divide the 32.768kHz clock source by divisor 32, where 2^(4 + 1): 32.768kHz/32=1.024kHz
                      GCLK_GENDIV_ID(2);              // Select Generic Clock (GCLK) 2
    while (GCLK->STATUS.bit.SYNCBUSY);                // Wait for synchronization
  
    REG_GCLK_GENCTRL = GCLK_GENCTRL_DIVSEL |          // Set to divide by 2^(GCLK_GENDIV_DIV(4) + 1)
                       GCLK_GENCTRL_IDC |             // Set the duty cycle to 50/50 HIGH/LOW
                       GCLK_GENCTRL_GENEN |           // Enable GCLK2
                       GCLK_GENCTRL_SRC_OSCULP32K |   // Set the clock source to the ultra low power oscillator (OSCULP32K)
                       GCLK_GENCTRL_ID(2);            // Select GCLK2         
    while (GCLK->STATUS.bit.SYNCBUSY);                // Wait for synchronization
  
    // Feed GCLK2 to WDT (Watchdog Timer)
    REG_GCLK_CLKCTRL = GCLK_CLKCTRL_CLKEN |           // Enable GCLK2 to the WDT
                       GCLK_CLKCTRL_GEN_GCLK2 |       // Select GCLK2
                       GCLK_CLKCTRL_ID_WDT;           // Feed the GCLK2 to the WDT
    while (GCLK->STATUS.bit.SYNCBUSY);                // Wait for synchronization
  
    REG_WDT_CONFIG = aConfig;                         // Set the WDT reset timeout to 1 second
    while(WDT->STATUS.bit.SYNCBUSY);                  // Wait for synchronization
    REG_WDT_CTRL = WDT_CTRL_ENABLE;                   // Enable the WDT in normal mode
    while(WDT->STATUS.bit.SYNCBUSY);                  // Wait for synchronization
  }

  inline void resetWatchdog() const {
    if (!WDT->STATUS.bit.SYNCBUSY) {                  // Check if the WDT registers are synchronized
      REG_WDT_CLEAR = WDT_CLEAR_CLEAR_KEY;            // Clear the watchdog timer
    }
  }
 
private:
  static App* pApp;

  Sensors sensors;
  Communication communication;
      
  static RTCZero rtc;

  String imei;

  enum ports {
    TRIGGER = 2,
    ECHO = 3,
    LED = 6,
    AM2302 = 0
  };

// Parameters
  Alert alert1, alert2;
  byte startTime, stopTime;
  int reset;            ///< reset time in min ou -1 if not.

  static volatile
  bool fIntTimer;

};

App* App::pApp;
RTCZero App::rtc;
volatile bool App::fIntTimer;
