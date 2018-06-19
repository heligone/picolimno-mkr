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
/// ATTENTION FONCTIONNE SEULEMENT AVEC LA v5.13.1 (un problème sur le 5.13.2 que je n'ai pas investigué)
#include <ArduinoJson.h>

#include "sensors.h"
#include "alert.h"

#define TINY_GSM_MODEM_UBLOX
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

/// Temps en secondes entre deux mesures de distance.
#define INTERVAL_MESURES (60)

/// Temps en seconde entre deux transmissions.
#define INTERVAL_TRANSMISSION (15*60)

/// Nombre d'échantillons matériels nécessaires pour faire un échantillon brut après médiane (minimum sinon l'échantillon est invalide).
#define RANGE_SEQ_MIN 10
/// Nombre maximum de mesures matérielles pou obtenir le nombre d'échantillons matériels nécessaires.
#define RANGE_SEQ_MAX 175

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

// GSM Card
// * set serial baudrate
    SerialGSM.begin(115200);
// * hard resert
    pinMode(GSM_RESETN, OUTPUT);
    digitalWrite(GSM_RESETN, HIGH);
    delay(100);
    digitalWrite(GSM_RESETN, LOW);

// Connection GSM & GPRS    
    DEBUG(F("Setting up GSM & GPRS connection...\n"));
    if (!connectGSMGPRS()) {
      DEBUG(F("Echec de connexion. Poursuite !\n"));
    }

    DEBUG(F("Getting IMEI... "));
    imei = modem.getIMEI();
    DEBUG(F("DeviceID: GSM-")); DEBUG(imei); DEBUG('\n');

    rtc.begin();

// Get Parameters & datetime
    DEBUG(F("Get parameters..."));
    const bool ok = getParameters();
    DEBUG(ok);
    DEBUG('\n');

// Sending Status
    DEBUG(F("Sending Status Starting\n"));
    if (!sendStatus(F("Starting"))) {
      if (!connectGSMGPRS()) {
        DEBUG(F("Echec de connexion. Poursuite !\n"));
      }
      if (!sendStatus(F("Starting"))) {
        DEBUG(F("Echec de retransmission. Poursuite !\n"));
      }    
    }

// Start all sensors (init...)
    if (!sensors.begin()) return false;

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
//    rtc.standbyMode();
    
// Si on a détecté un changement de minute
    if (App::fIntTimer) {
      const byte sec = rtc.getSeconds();
      const byte minu = rtc.getMinutes();
      const byte heure = rtc.getHours();
      const unsigned long t = sec + 60U * (minu + 60U * heure);
      App::fIntTimer = false;
  
      DEBUG(F("Wakeup @ "));
      DEBUG(getTimestamp());
      DEBUG("\n");

// Vérification de l'heure de RESET quotidien
      if ((reset >= 0) && (static_cast<unsigned>(reset) == minu + 60U * heure)) {
        NVIC_SystemReset();
      }

// Vérification de la période de veille
      if ((startTime > 0) && (heure < startTime)) return true;      // Pas encore l'heure (veille)
      if ((stopTime > 0) && (heure >= stopTime)) return true;       // Trop tard (veille)
      
// Présence d'un interval pour déclencher une mesure de distance
      if ((t % INTERVAL_MESURES) && (t % INTERVAL_TRANSMISSION)) return true;  // pas de mesure à cette minute

// Mesure de distance
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
            
      const unsigned distance = (n >= RANGE_SEQ_MIN ? d[n / 2] : 0);
      DEBUG(F("Distance : ")); DEBUG(distance / 10.0f); DEBUG(F(" - Ech. : ")); DEBUG(n); DEBUG('\n');

      if (distance > 0) {   // Pas d'alerte en cas de valeur à 0
        if (alert1.enabled() && alert1.test(distance / 10.0f)) {    // Alerte activée et dépassement de seuil (montant ou descendant)
          const sample_t sample = { rtc.getEpoch(), F("alert1"), distance / 10.0f };
          if (!sendSample(sample)) {
            if (!connectGSMGPRS()) {
              DEBUG(F("Echec de connexion. Poursuite !\n"));
            }
            if (!sendSample(sample)) {
              DEBUG(F("Echec de retransmission. Poursuite !\n"));
            }    
          }
        }
  
        if (alert2.enabled() && alert2.test(distance / 10.0f)) {    // Alerte activée et dépassement de seuil (montant ou descendant)
          const sample_t sample = { rtc.getEpoch(), F("alert2"), distance / 10.0f };
          if (!sendSample(sample)) {
            if (!connectGSMGPRS()) {
              DEBUG(F("Echec de connexion. Poursuite !\n"));
            }
            if (!sendSample(sample)) {
              DEBUG(F("Echec de retransmission. Poursuite !\n"));
            }    
          }
        }
      } else {  // Transmettre une trame d'erreur (distance invalide)
          const sample_t sample = { rtc.getEpoch(), F("invalide range"), 0};
          if (!sendSample(sample)) {
            if (!connectGSMGPRS()) {
              DEBUG(F("Echec de connexion. Poursuite !\n"));
            }
            if (!sendSample(sample)) {
              DEBUG(F("Echec de retransmission. Poursuite !\n"));
            }    
          }
      }
      
      if ((t % INTERVAL_TRANSMISSION) == 0) {   // C'est le moment de transmission
        sample_t samples[4];
        size_t s = 0;
        
        if (distance > 0) { // Ne pas transmettre de mesure invalide.
          samples[s] = { rtc.getEpoch(), F("range"), distance / 10.0f };  // Utilisation du dernier échantillon (valide).
#ifdef PETITES_TRAMES
          if (!sendSample(samples[s])) {
            if (!connectGSMGPRS()) {
              DEBUG(F("Echec de connexion. Poursuite !\n"));
            }
            if (!sendSample(samples[s])) {
              DEBUG(F("Echec de retransmission. Poursuite !\n"));
            }    
          }
#endif
          ++s;
        }

        float temp, hygro = 0.0;
        if (sensors.sampleAM2302(temp, hygro)) {
          samples[s] = { rtc.getEpoch(), F("temp"), temp };
#ifdef PETITES_TRAMES
          if (!sendSample(samples[s])) {
            if (!connectGSMGPRS()) {
              DEBUG(F("Echec de connexion. Poursuite !\n"));
            }
            if (!sendSample(samples[s])) {
              DEBUG(F("Echec de retransmission. Poursuite !\n"));
            }    
          }
#endif
          ++s;
          DEBUG(F("Temperature : ")); DEBUG(temp); DEBUG('\n');
          
          samples[s] = { rtc.getEpoch(), F("hygro"), hygro };
#ifdef PETITES_TRAMES
          if (!sendSample(samples[s])) {
            if (!connectGSMGPRS()) {
              DEBUG(F("Echec de connexion. Poursuite !\n"));
            }
            if (!sendSample(samples[s])) {
              DEBUG(F("Echec de retransmission. Poursuite !\n"));
            }    
          }
#endif
          ++s;
          DEBUG(F("Hygrometrie : ")); DEBUG(hygro); DEBUG('\n');
        }
        const float vBat = sensors.sampleBattery();
        samples[s] = { rtc.getEpoch(), F("vbat"), vBat };
#ifdef PETITES_TRAMES
          if (!sendSample(samples[s])) {
            if (!connectGSMGPRS()) {
              DEBUG(F("Echec de connexion. Poursuite !\n"));
            }
            if (!sendSample(samples[s])) {
              DEBUG(F("Echec de retransmission. Poursuite !\n"));
            }    
          }
#endif
        ++s;
        DEBUG("Batterie : "); DEBUG(vBat); DEBUG('\n');

#ifndef PETITES_TRAMES
        if (!sendSamples(samples, s)) {
          if (!connectGSMGPRS()) {
            DEBUG(F("Echec de connexion. Poursuite !\n"));
          }
          if (!sendSamples(samples, s)) {
            DEBUG(F("Echec de retransmission. Poursuite !\n"));
          }    
        }
#endif
// Get Parameters
        DEBUG(F("Get parameters..."));
        const bool ok = getParameters();
        DEBUG(ok);
        DEBUG('\n');
      }
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
    sensors(TRIGGER, ECHO, AM2302),   ///< Initialisation de capteurs (broches de connexion)
    GPRS_APN(apn), 
    GPRS_LOGIN(login),
    GPRS_PASSWORD(password),
    modem(SerialGSM),                 ///< Initialisation de la carte modem GSM.
    serverName(F("api.picolimno.fr")),
    serverPort(80),
    alert1(true),                         ///< Initialisation de l'alerte Orange (seuil et hystérésis) avec inversion
    alert2(true),                         ///< Initialisation de l'alerte Rouge  (seuil et hystérésis) avec inversion
    startTime(0),                         ///< Heure de démarrage des mesures (HH) 
    stopTime(0),                          ///< Heure de fin des mesures (HH)
    reset(-1)
  {
  }

/**
 * Connecte ou reconnecte le GSM et le GPRS selon les besoins.
 * 
 * @param retry Nombre de réessais pour obtenir les 2 connexions successives, 10 par défaut.
 * @return L'état de la connexion, true si ok, false en cas d'erreur.
 */
  bool connectGSMGPRS(const byte retry = 10) {
    DEBUG(F("GSM...\n"));

    if (!modem.restart()) {
      DEBUG(F("Error restarting modem!\n"));
      return false;
    }

    const String info = modem.getModemInfo();
    DEBUG(F("TinyGSM using ")); DEBUG(info); DEBUG('\n');

    const String SCCID = modem.getSimCCID();
    DEBUG(F("Sim CCID ")); DEBUG(SCCID);
    const SimStatus ss = modem.getSimStatus();
    DEBUG(F(", Status ")); DEBUG(ss == 0 ? F("ERROR!") : ss == 1 ? F("READY.") : ss == 2 ? F("LOCKED!") : F("unknown!")); DEBUG('\n');

    const int battLevel = modem.getBattPercent();
    DEBUG("Battery level: "); DEBUG(battLevel); DEBUG('\n');
    
    DEBUG(F("Waiting for network... "));
    if (!modem.waitForNetwork()) {
      DEBUG(F("Not found!\n"));
      return false;
    }
  
    if (modem.isNetworkConnected()) {
      DEBUG(F("connected with Operator "));
      DEBUG(modem.getOperator());
      DEBUG(F(",Signal "));
      DEBUG(modem.getSignalQuality());
      DEBUG('\n');
    }
  
    DEBUG(F("GPRS (")); DEBUG(GPRS_APN); DEBUG(F(")...\n"));
    bool ok;
    for (byte i = 0; i < retry; ++i) {
      ok = modem.gprsConnect(String(GPRS_APN).c_str(), String(GPRS_LOGIN).c_str(), String(GPRS_PASSWORD).c_str());
      if (!ok) {
        DEBUG(F("fail, "));
        delay(500);
      } else {
        DEBUG(F("OK\n"));
        break;
      }
    }
    if (!ok) {
      DEBUG(F("Not found!\n"));
      return false;
    }

    return true;  // Connexion avec succès
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
 * Transmet l'état du device sous la forme d'un flux Json qui contient le statut ainsi que d'autres éléments :
 * - L'heure mémorisée au moment de la transmission ;
 * - L'état tel que passé en paramètre ;
 * - L'IP du périphérique ;
 * 
 * @param aState L'état transmis dans le flux Json.
 * @return Le succès de la transmission, ou pas.
 */
  bool sendStatus(const String& aState) {
    const String path = String(F("/device/GSM-")) + imei + F("/status");

    String json(F("{\"timestamp\": \"")); 
    json += getTimestamp();
    json += F("\",\"status\":\"");
    json += aState;
    json += F("\",\"IP\":\"");
    json += modem.getLocalIP();
    json += F("\"}");

    TinyGsmClient client(modem);
    if (!client.connect(serverName.c_str(), serverPort)) {
      DEBUG(F("No connection in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("!"));
      return false;
    }

    HttpClient http = HttpClient(client, serverName, serverPort);
    int status;
    
    for (int i = 0; i < 3; ++i) {
      const int err = http.put(path, String(F("application/json")), json);
      if (err != 0) {
        DEBUG(F("Error on PUT (")); DEBUG(err); DEBUG(F(")!\n"));
        break;
      }
      status = http.responseStatusCode();
      if (status <= 0) {
        DEBUG(F("Internal error on PUT (")); DEBUG(status); DEBUG(F(")!\n'"));
        delay(500);
        continue; // un autre essai!
      } else {
        DEBUG(F("HTTP Response : ")); DEBUG(status); DEBUG('\n');
        break;
      }
    }
    if (status <= 0) return false;

    while (!http.endOfHeadersReached()) {
      if (http.headerAvailable()) {
        const String name = http.readHeaderName();
        const String value = http.readHeaderValue();
        DEBUG(F("Header ")); DEBUG(name); DEBUG(':'); DEBUG(value); DEBUG('\n');
      }
    }
    const String body = http.responseBody();
    DEBUG(F("Body: ")); DEBUG(body); DEBUG('\n');

    client.stop();
    return true;    
  }
  
/**
 * Transmet un échantillon sample_t sérialisé sous la forme JSON d'un tableau d'un seul élément.
 * @param sample L'échantillon à traduire en JSON avant de le transmettre.
 * @return Le succès de la transmission, ou pas.
 */
  bool sendSample(const sample_t& sample) {
    return sendSamples(&sample, 1);
  }

/**
 * Transmet plusieurs échantillons sample_t sérialisés sous la forme JSON d'un tableau d'éléments.
 * @param samples Les échantillons à traduire en JSON avant de les transmettre. Tout le tableau est transmis.
 * @return Le succès de la transmission, ou pas.
 */
  template<size_t N>
  bool sendSamples(const sample_t (&samples)[N]) {
    return sendSamples(samples, N);
  };
  
/**
 * Transmet plusieurs échantillons sample_t sérialisés sous la forme JSON d'un tableau d'éléments.
 * @param samples Les échantillons à traduire en JSON avant de les transmettre.
 * @param n Le nombre d'échantillons du tableau à transmettre (premiers).
 * @return Le succès de la transmission, ou pas.
 */
  bool sendSamples(const sample_t samples[], const size_t n) {
    const String path = String(F("/device/GSM-")) + imei + F("/samples");

    String json('[');
    for (size_t i = 0; i < n; ++i) {
      const sample_t& sample = samples[i];
      json += (i ? F(",{") : F("{"));
      json += F("\"epoch\":\"");
      json += sample.epoch;
      json += F("\",\"key\":\"");
      json += sample.variable;
      json += F("\",\"value\":\"");
      json += String(sample.value);
      json += F("\"}");
    }
    json += ']';
    DEBUG(json); DEBUG('\n');

    TinyGsmClient client(modem);;
    if (!client.connect(serverName.c_str(), serverPort)) {
      DEBUG(F("No connection in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("!"));
      return false;
    }

    HttpClient http = HttpClient(client, serverName, serverPort);
    int status;
    
    for (int i = 0; i < 3; ++i) {
      const int err = http.put(path, String(F("application/json")), json);
      if (err != 0) {
        DEBUG(F("Error on PUT (")); DEBUG(err); DEBUG(F(")!\n"));
        break;
      }
      status = http.responseStatusCode();
      if (status <= 0) {
        DEBUG(F("Internal error on PUT (")); DEBUG(status); DEBUG(F(")!\n'"));
        delay(500);
        continue; // un autre essai!
      } else {
        DEBUG(F("HTTP Response : ")); DEBUG(status); DEBUG('\n');
        break;
      }
    }
    if (status <= 0) return false;
    
    while (!http.endOfHeadersReached()) {
      if (http.headerAvailable()) {
        const String name = http.readHeaderName();
        const String value = http.readHeaderValue();
        DEBUG(F("Header ")); DEBUG(name); DEBUG(':'); DEBUG(value); DEBUG('\n');
      }
    }
    const String body = http.responseBody();
    DEBUG(F("Body: ")); DEBUG(body); DEBUG('\n');

    client.stop();
    return true;    
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
 * Requete la liste des parametres.
 * 
 * @return Une chaîne JSON contenant chaque paramètre et sa valeur.
 */
  bool getParameters() {
    const String path = String(F("/device/GSM-")) + imei + F("/parameters");

    TinyGsmClient client(modem);
    if (!client.connect(serverName.c_str(), serverPort)) {
      DEBUG(F("No connection in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("!"));
      return false;
    }

    HttpClient http = HttpClient(client, serverName, serverPort);
    int status;
    
    for (int i = 0; i < 3; ++i) {
      const int err = http.get(path);
      if (err != 0) {
        DEBUG(F("Error on GET (")); DEBUG(err); DEBUG(F(")!\n"));
        break;
      }
      status = http.responseStatusCode();
      if (status <= 0) {
        DEBUG(F("Internal error on GET (")); DEBUG(status); DEBUG(F(")!\n'"));
        delay(500);
        continue; // un autre essai!
      } else {
        DEBUG(F("HTTP Response : ")); DEBUG(status); DEBUG('\n');
        break;
      }
    }
    if (status <= 0) return false;

    while (!http.endOfHeadersReached()) {
      if (http.headerAvailable()) {
        const String name = http.readHeaderName();
        const String value = http.readHeaderValue();
        DEBUG(F("Header ")); DEBUG(name); DEBUG(':'); DEBUG(value); DEBUG('\n');

        if (name.equalsIgnoreCase(F("Date"))) {
          struct tm tm;
          strptime(value.c_str(),"%a, %e %h %Y %H:%M:%S %z",&tm);
          rtc.setTime(tm.tm_hour, tm.tm_min, tm.tm_sec);
          rtc.setDate(tm.tm_mday, tm.tm_mon + 1, tm.tm_year % 100);
        }
      }
    }
    
    const String body = http.responseBody();
    DEBUG(F("Body: ")); DEBUG(body); DEBUG('\n');
    client.stop();

    DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(6) + 60);
    const JsonObject& root = jsonBuffer.parseObject(body);

    if (root.containsKey("limit1R") && root.containsKey("hyst1R")) {
      alert1 = Alert(root["limit1R"], root["hyst1R"]);
    }
    if (root.containsKey("limit2O") && root.containsKey("hyst2O")) {
      alert2 = Alert(root["limit2O"], root["hyst2O"]);
    }

    startTime = root["start"];
    stopTime = root["stop"];

    if (root.containsKey("reset")) {
      const String s = root["reset"];
      const byte hh = s.toInt();
      const byte mm = s.substring(s.indexOf(':')+1).toInt();
      reset = hh * 60U + mm;
    } else reset = -1;

    return true;
  }
 
private:
  static App* pApp;

  Sensors sensors;

  const __FlashStringHelper* GPRS_APN;
  const __FlashStringHelper* GPRS_LOGIN;
  const __FlashStringHelper* GPRS_PASSWORD;

  TinyGsm modem;
  const String serverName;
  const unsigned serverPort;
      
  static RTCZero rtc;

  String imei;

  enum ports {
    TRIGGER = 2,
    ECHO = 3,
    LED = 6,
    AM2302 = 0
  };

/**
 * Structure des éléments enregistrés dans la file d'attente des mesures.
 */
  struct sample_t {
    uint32_t epoch;
    const __FlashStringHelper* variable;  // 
    float value;
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

