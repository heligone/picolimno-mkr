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

#include <MKRGSM.h>
#include <RTCZero.h>
#include <ctime>
#include <assert.h>
#include <QueueArray.h>

#include "sensors.h"

/**
 * Classe principale qui implémente l'application.
 */
class App {
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
 * C'est une machine à états (https://fr.wikipedia.org/wiki/%C3%89tat_(patron_de_conception).
 * @todo A implémenter dans les règles de l'art, pas avec juste une boucle et un switch.
 * 
 * @return boolean value to indicate if execution was right or not. A faulty exec. means the program can't continue and should be aborted.
 */
  bool setup() {
    enum { STATE_IDLE, STATE_GSM_READY, STATE_GSM_ERROR, STATE_GPRS_READY, STATE_GPRS_ERROR, STATE_READY } state = STATE_IDLE;

    while (true) {    // lancement de la machine a états
      switch (state) {
        case STATE_IDLE : {   // démarrage, connexion GSM...
          modem.begin();
          DEBUG(F("Setting up GSM connection..."));
          int err;
          for (byte i = 0; i < 10; ++i) {
            err = gsmAccess.begin();
            if (err != GSM_READY) {
              DEBUG(F(" Not connected [err="));
              DEBUG(err);
              DEBUG(F("]..."));
              delay(500);
            } else break;
          };
          if (err != GSM_READY) {
            DEBUG(F(" Timeout!")); DEBUG('\n');
            state = STATE_GSM_ERROR;
          } else {
            DEBUG(F(" Connected.\n"));
            state = STATE_GSM_READY;
          }
          break;
        }
        case STATE_GSM_READY : {  // gsm actif et récupération de l'emei et de l'heure puis connexion GPRS...
          do {
            imei = modem.getIMEI();
          } while (imei.length() == 0);
          DEBUG(F("DeviceID: GSM-")); DEBUG(imei); DEBUG('\n');
          
          rtc.begin();
          const time_t t = gsmAccess.getTime();
          struct tm *const stm = gmtime(&t);
          rtc.setTime(stm->tm_hour, stm->tm_min, stm->tm_sec);
          rtc.setDate(stm->tm_mday, stm->tm_mon + 1, stm->tm_year % 100);
          DEBUG(F("Setting time at ")); DEBUG(getTimestamp()); DEBUG('\n');
          
          DEBUG(F("Setting up GPRS connection..."));
          if (connectGPRS()) {
            DEBUG(F(" Connected.")); DEBUG('\n');
            state = STATE_GPRS_READY;
          } else {
            DEBUG(F(" Timeout!")); DEBUG('\n');
            state = STATE_GPRS_ERROR; 
          }
          break;
        }
        case STATE_GPRS_READY : {   // GPRS ok, envoi d'un status de démarrage
          DEBUG(F("Sending Status Starting")); DEBUG('\n');
          sendStatus(F("Starting"));
          state = STATE_READY;
          break;
        }
        case STATE_READY : {  // Etat nominal, tous systèmes démarrés
          if (!sensors.begin()) return false;
          
          pinMode(LED, OUTPUT);
      
//          const byte sec = rtc.getSeconds();
//          rtc.setAlarmSeconds((sec + (10 - sec % 10)) % 60);  // programmation de la prochaine alarme dans 10 sec.
          rtc.setAlarmSeconds(0);  // programmation de la prochaine alarme au début de la minute
          rtc.enableAlarm(rtc.MATCH_SS);
          rtc.attachInterrupt(oneMinute);
          fOneMinute = false;
          
          return true;
        }          
        default :
          return false;
      }
    }
  }

/**
 * Loop and executing all application's logic.
 * 
 * @return boolean value to indicate if execution was right or not. A faulty exec. means the program can't continue and should be aborted.
 */
  bool loop() {
/*    
    DEBUG(mesurerDistance());
    DEBUG('\n');
    return true;
*/    

    static unsigned last_distance;
    
//    rtc.standbyMode();
    
// Si on a détecté un changement de minute, on lance les mesures nécessaires
    if (App::fOneMinute) {
      DEBUG(F("Wakeup @ "));
      DEBUG(getTimestamp());
      DEBUG("\n");
//      const byte sec = rtc.getSeconds();
      const byte minu = rtc.getMinutes();
      App::fOneMinute = false;
//      rtc.setAlarmSeconds((sec + (9 - sec % 10)) % 60);

      unsigned d[11];
      for (int i = 10; i >= 0; --i) {
        d[i] = sensors.sampleRange();
      }
      insertionSortR(d, 11);
      const unsigned distance = d[5];
      const sample_t sample = { rtc.getEpoch(), F("range"), distance / 10.0f };
      queue.push(sample);
      DEBUG(distance / 10.0); DEBUG('\n');
  
      if (!(minu % 15)) { // tous les 1/4 d'heure
        float temp, hygro = 0;
        if (sensors.sampleAM2302(temp, hygro)) {
          const sample_t sample1 = { rtc.getEpoch(), F("temp"), temp };
          queue.push(sample1);
          
          const sample_t sample2 = { rtc.getEpoch(), F("hygro"), hygro };
          queue.push(sample2);
        }
      }
      
      if (!(minu % 15)) {   // tous les 1/4 d'heure
        const sample_t sample3 = { rtc.getEpoch(), F("vbat"), sensors.sampleBattery() };
        queue.push(sample3);
      }
            
#define ALERT_LEVEL 500
      const bool alert = (last_distance < ALERT_LEVEL) &&  (distance >= ALERT_LEVEL);
      last_distance = distance;

// Transmission des données si quantité suffisante ou alerte.
      if ((queue.count() >= App::QUEUE_DEPTH) || alert) {  
        String json;
        while (!queue.isEmpty()) {
          const sample_t sample = queue.pop();
          json.concat(json.length() ? F("\"},{") : F("[{"));
          json.concat(F("\"epoch\":\""));
          json.concat(sample.epoch);
          json.concat(F("\",\"key\":\""));
          json.concat(sample.variable);
          json.concat(F("\",\"value\":\""));
          json.concat(sample.value);
        }
        json.concat(F("\"}]"));
//        DEBUG(json); DEBUG("\n");
  
        const String path = String(F("/device/GSM-")) + imei + F("/samples");
        if (!put(path, json)) {
          Serial.println(F("Erreur de PUT"));
          return false;
        }

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
    sensors(TRIGGER, ECHO, AM2302),
    GPRS_APN(apn), 
    GPRS_LOGIN(login),
    GPRS_PASSWORD(password),
    SERVER_HOST(F("api.picolimno.fr")),
    SERVER_PORT(80)
  {
  }

/**
 * Called very one second by interruption.
 * Wake the CPU up.
 */
  static 
  void oneMinute() {
    fOneMinute = true;
  }

/** 
 * Assure la connection au réseau GPRS et retourne le succès de l'opération.
 * Il y a un timeout de 5s pour essayer d'assurer la connection.
 *
 * @return L'état de la connection à l'issue de la tentative.
 */
  bool connectGPRS() {
    int err;
    for (byte i = 0; i < 10; ++i) {
      err = gprs.attachGPRS(GPRS_APN.c_str(), GPRS_LOGIN.c_str(), GPRS_PASSWORD.c_str());
      if (err != GPRS_READY) {
        DEBUG(F(" Not connected [err="));
        DEBUG(err);
        DEBUG(F("]..."));
        delay(500);            
      } else break;
    }
    return (err == GPRS_READY);
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

    String payload(F("{\"timestamp\": \"")); 
    payload += getTimestamp();
    payload += F("\",\"status\":\"");
    payload += aState;
    payload += F("\",\"IP\":\"");
    IPAddress LocalIP = gprs.getIPAddress();
    payload += LocalIP[0];
    payload += '.';
    payload += LocalIP[1];
    payload += '.';
    payload += LocalIP[2];
    payload += '.';
    payload += LocalIP[3];
    payload += F("\"}");

    if (!put(path, payload)) {
      Serial.println(F("Erreur de PUT"));
      return false;
    }
    
    return true;    
  }

/**
 * Émet une requête HTTP PUT vers le serveur de référence.
 * 
 * @param aPath Chemin transmis dans la requête.
 * @param aBody Contenu de la requête.
 * @return Le succès de la transmission.
 */
  bool put(const String& aPath, const String& aBody) {
    GSMClient client;

    if (!client.connect(SERVER_HOST.c_str(), SERVER_PORT)) {
      Serial.println(F("Erreur de connexion"));
      return false;
    }
    
    client.print(F("PUT "));   client.print(aPath);   client.println(F(" HTTP/1.0"));
    client.print(F("Host: ")); client.println(SERVER_HOST);
    client.println(F("Content-Type: application/json"));
    client.print(F("Content-Length: ")); client.println(aBody.length());
    client.println(F("Connection: close"));
    client.println();

    String t = aBody;
    while(t.length() > 0) {
      const unsigned m = (t.length() >= 100 ? 100 : t.length());
      client.print(t.substring(0, m));
      t = t.substring(m);
    }

    const unsigned long start = millis();
    while ((client.available() || client.connected()) && (millis() - start) < 30000) {
      if (client.available() > 0) {
        const int c = client.read();
        if (c != -1) Serial.print(char(c));
      }
    }
    client.stop();
    return true;
  }
  
/**
 * Retourne la date et l'heure maintenues par la RTC locale.
 * 
 * @return L'heure actuelle au format ISO3339 (https://www.ietf.org/rfc/rfc3339.txt).
 */
  String getTimestamp() {
    char buffer[25];
    snprintf(buffer, sizeof(buffer), "20%02d-%02d-%02dT%02d:%02d:%02dZ", rtc.getYear(), rtc.getMonth(), rtc.getDay(), rtc.getHours(), rtc.getMinutes(), rtc.getSeconds() );
    return String(buffer);
  }

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

private:
  static App* pApp;

  Sensors sensors;

  const String GPRS_APN;
  const String GPRS_LOGIN;
  const String GPRS_PASSWORD;
  const String SERVER_HOST;
  const unsigned SERVER_PORT;

  GSMModem modem;
  GPRS gprs;
  GSM gsmAccess;

  static RTCZero rtc;

  String imei;

  enum ports {
    TRIGGER = 2,
    ECHO = 3,
    LED = 6,
    AM2302 = 0
  };

  static const int QUEUE_DEPTH;

  struct sample_t {
    uint32_t epoch;
    const __FlashStringHelper* variable;  // 
    float value;
  };

  static 
  QueueArray<sample_t> queue;

  static volatile
  bool fOneMinute;

};

App* App::pApp;
RTCZero App::rtc;
QueueArray<App::sample_t> App::queue;
volatile bool App::fOneMinute;
const int App::QUEUE_DEPTH = 10;


