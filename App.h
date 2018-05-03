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
#include "parameters.h"
// #include "http.h"
#include <ArduinoHttpClient.h>


// Temps en secondes entre deux mesures de distance
#define INTERVAL_MESURES 10
#define INTERVAL_PARAMS (15*60U)


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
 * C'est une machine à états (https://fr.wikipedia.org/wiki/%C3%89tat_(patron_de_conception).
 * @todo A implémenter dans les règles de l'art, pas avec juste une boucle et un switch.
 * 
 * @return boolean value to indicate if execution was right or not. A faulty exec. means the program can't continue and should be aborted.
 */
  bool setup() {

// Connection GSM & GPRS    
    DEBUG(F("Setting up GSM connection..."));
    MODEM.begin();
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
      DEBUG(F(" Timeout! No GSM found!\n"));
      return false;
    } else {
      DEBUG(F(" Connected.\n"));
    }

    DEBUG(F("Getting IMEI..."));
    do {
      GSMModem modem;
      imei = modem.getIMEI();
      delay(500);
    } while (imei.length() == 0);
    DEBUG(F(" DeviceID: GSM-")); DEBUG(imei); DEBUG('\n');

    DEBUG(F("Setting up GPRS connection..."));
    if (connectGPRS()) {
      DEBUG(F(" Connected.")); DEBUG('\n');
    } else {
      DEBUG(F(" Timeout!")); DEBUG(F(" No GPRS connection! \n"));
      return false;
    }

// Get Date NTP    
    DEBUG(F("Requesting NTP date... "));
    const uint32_t epoch = getNTP();
    struct tm *const stm = gmtime((long*)&epoch);
    if ((1900 + stm->tm_year) >= ((__DATE__[7] - 0x30) * 1000 + (__DATE__[8] - 0x30) * 100 + (__DATE__[9] - 0x30) * 10 + (__DATE__[10] - 0x30))) { // année cohérante
      rtc.begin();
      rtc.setTime(stm->tm_hour, stm->tm_min, stm->tm_sec);
      rtc.setDate(stm->tm_mday, stm->tm_mon + 1, stm->tm_year % 100);
      DEBUG(getTimestamp());
      DEBUG(F("(UTC) - Succeded.\n"));
    } else {
      DEBUG(F("Warning no NTP date!\n"));
      return false;
    }

// Sending Status
    DEBUG(F("Sending Status Starting\n"));
    sendStatus(F("Starting"));

// Get parameters    
//    DEBUG(F("Requesting parameters")); DEBUG('\n');
//    const String s = getParameters();
    
    if (!sensors.begin()) return false;
          
//    pinMode(LED, OUTPUT);

// Initiat. ranges data set    
    for (byte i = 0; i < 15; ++i) ranges[i] = sensors.sampleRange();

// Define next timer's interrupt
    DEBUG(F("Start timer.\n"));
    const byte sec = rtc.getSeconds();
    const byte minu = rtc.getMinutes();
    const unsigned t = minu * 60U + sec + INTERVAL_MESURES;
    rtc.setAlarmSeconds(t % 60);
    rtc.setAlarmMinutes((t / 60) % 60);

    rtc.enableAlarm(rtc.MATCH_MMSS);
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
    static byte nbEchant = 0;
//    rtc.standbyMode();
    
// Si on a détecté un changement de minute, on lance les mesures nécessaires
    if (App::fIntTimer) {
      DEBUG(F("Wakeup @ "));
      DEBUG(getTimestamp());
      DEBUG("\n");
// Calcul de la prochaine interruption
      const byte sec = rtc.getSeconds();
      const byte minu = rtc.getMinutes();
      const unsigned t = minu * 60 + sec + INTERVAL_MESURES - 1;
      App::fIntTimer = false;
      rtc.setAlarmSeconds(t % 60);
      rtc.setAlarmMinutes((t / 60) % 60);

      unsigned d[5];
      for (byte i = 0; i < 5; ++i) {
        d[i] = sensors.sampleRange();
      }
      insertionSortR(d, 3);
      const unsigned distance = d[3];
      DEBUG(F("Distance : ")); DEBUG(distance / 10.0f); DEBUG('\n');
  
      for (byte i = 0; i < 15; ++i) {
        ranges[i] = (i < 14 ? ranges[i + 1] : distance);
        DEBUG(ranges[i]); DEBUG('-');
      }
      DEBUG('\n');
      nbEchant = (nbEchant + 1) % 15;
      if (!nbEchant) {
        unsigned e[15];
        memcpy(e, ranges, sizeof(unsigned)*15);
        insertionSortR(e, 7);
        const unsigned distance = e[7];
        const sample_t sample = { rtc.getEpoch(), F("range"), distance / 10.0f };
        sendSample(sample);

        float temp, hygro = 0.0;
        if (sensors.sampleAM2302(temp, hygro)) {
          const sample_t sample1 = { rtc.getEpoch(), F("temp"), temp };
          sendSample(sample1);
          DEBUG(F("Temperature : ")); DEBUG(temp); DEBUG('\n');
          
          const sample_t sample2 = { rtc.getEpoch(), F("hygro"), hygro };
          sendSample(sample2);
          DEBUG(F("Hygrometrie : ")); DEBUG(hygro); DEBUG('\n');
        }
        const float vBat = sensors.sampleBattery();
        const sample_t sample3 = { rtc.getEpoch(), F("vbat"), vBat };
        sendSample(sample3);
        DEBUG("Batterie : "); DEBUG(vBat); DEBUG('\n');
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
    parameters(),
//    http(F("api.picolimno.fr"), 443),
    GPRS_APN(apn), 
    GPRS_LOGIN(login),
    GPRS_PASSWORD(password)
  {
  }

/**
 * Retourne l'heure à partir d'une requête NTP.
 *
 * @param aURL l'url du service ntp utilisé.
 * @return Le temps epoch écoulé.
 */

  uint32_t getNTP() {
    struct __attribute__ ((packed)) NtpPacket { 
      uint8_t li_vn_mode;      // Eight bits. li, vn, and mode.
                               // li.   Two bits.   Leap indicator.
                               // vn.   Three bits. Version number of the protocol.
                               // mode. Three bits. Client will pick mode 3 for client.
    
      uint8_t stratum;         // Eight bits. Stratum level of the local clock.
      uint8_t poll;            // Eight bits. Maximum interval between successive messages.
      uint8_t precision;       // Eight bits. Precision of the local clock.
    
      uint32_t rootDelay;      // 32 bits. Total round trip delay time.
      uint32_t rootDispersion; // 32 bits. Max error aloud from primary clock source.
      uint32_t refId;          // 32 bits. Reference clock identifier.
    
      uint32_t refTm_s;        // 32 bits. Reference time-stamp seconds.
      uint32_t refTm_f;        // 32 bits. Reference time-stamp fraction of a second.
    
      uint32_t origTm_s;       // 32 bits. Originate time-stamp seconds.
      uint32_t origTm_f;       // 32 bits. Originate time-stamp fraction of a second.
    
      uint32_t rxTm_s;         // 32 bits. Received time-stamp seconds.
      uint32_t rxTm_f;         // 32 bits. Received time-stamp fraction of a second.
    
      uint8_t txTm_s[4];
//      uint32_t txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
      uint32_t txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.
    };
    NtpPacket packetBuffer;
    
    assert(sizeof(packetBuffer) == 48);
    memset(&packetBuffer, 0, sizeof(packetBuffer));
    packetBuffer.li_vn_mode = 0b11100011;
    packetBuffer.stratum = 0;
    packetBuffer.poll = 6;
    packetBuffer.precision = 0xEC;

    GSMUDP Udp;

    Udp.begin(2390); // listening 

    IPAddress timeServer;
    if (!gprs.hostByName(F("fr.pool.ntp.org"), timeServer)) {
      timeServer = IPAddress(163,172,12,49);
    }

    Udp.beginPacket(timeServer, 123);
    Udp.write((const char*)&packetBuffer, sizeof(packetBuffer));
    Udp.endPacket();

    unsigned long start = millis();
    do {
      if ( Udp.parsePacket() ) {
        Udp.read((char*)&packetBuffer, sizeof(packetBuffer));
        return ((packetBuffer.txTm_s[0] * 0x100UL + packetBuffer.txTm_s[1]) * 0x100UL + packetBuffer.txTm_s[2]) * 0x100UL + packetBuffer.txTm_s[3] - 2208988800UL;
      } else {
        DEBUG('.');
        delay(500);
      }
    } while (millis() - start < 30000UL);

    return 0;    
  };


/**
 * Called every Timer's interruption.
 * Wake the CPU up.
 */
  static 
  void intTimer() {
    App::fIntTimer = true;
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

    const String serverName = String(F("api.picolimno.fr"));
    const unsigned serverPort = 80;

    GSMClient client;
    if (!client.connect(serverName.c_str(), serverPort)) {
      DEBUG(F("No connection !"));
      return false;
    }

    HttpClient http = HttpClient(client, serverName, serverPort);
    const int err = http.put(path, String(F("application/json")), payload);
    if (err != 0) {
      DEBUG(F("Error on PUT (")); DEBUG(err); DEBUG(F(")!\n"));
      return false;
    }
    const int status = http.responseStatusCode();
    if (status <= 0) {
      DEBUG(F("Internal error on PUT")); DEBUG(status); DEBUG('\n');
      return false;
    } else if (status < 200 && status >= 300) {
      DEBUG(F("HTTP Response : ")); DEBUG(status); DEBUG('\n');
      return false;
    } else {
      DEBUG(F("Success (")); DEBUG(status); DEBUG(F(")\n"));
    }

    while (!http.endOfHeadersReached()) {
      if (http.headerAvailable()) {
        const String name = http.readHeaderName();
        const String value = http.readHeaderValue();
        DEBUG(F("Header ")); DEBUG(name); DEBUG(':'); DEBUG(value); DEBUG('\n');
      }
    }
    const String body = http.responseBody();
    DEBUG(F("Body: ")); DEBUG(body); DEBUG('\n');
      
    return true;    
  }
  
/**
 * Transmet un échantillon sample_t sérialisé sous la forme JSON d'un tableau d'un seul élément.
 * @param sample L'échantillon à traduire en JSON avant de le transmettre.
 * @return Le succès de la transmission, ou pas.
 */
  bool sendSample(const sample_t& sample) {
    const String path = String(F("/device/GSM-")) + imei + F("/samples");

    String json(F("[{"));
    json += F("\"epoch\":\"");
    json += sample.epoch;
    json += F("\",\"key\":\"");
    json += sample.variable;
    json += F("\",\"value\":\"");
    json += String(sample.value);
    json += F("\"}]");
    DEBUG(json); DEBUG('\n');

    const String serverName = String(F("api.picolimno.fr"));
    const unsigned serverPort = 80;

    GSMClient client;
    if (!client.connect(serverName.c_str(), serverPort)) {
      DEBUG(F("No connection !"));
      return false;
    }

    HttpClient http = HttpClient(client, serverName, serverPort);
    const int err = http.put(path, String(F("application/json")), json);
    if (err != 0) {
      DEBUG(F("Error on PUT (")); DEBUG(err); DEBUG(F(")!\n"));
      return false;
    }
    const int status = http.responseStatusCode();
    if (status <= 0) {
      DEBUG(F("Internal error on PUT")); DEBUG(status); DEBUG('\n');
      return false;
    } else if (status < 200 && status >= 300) {
      DEBUG(F("HTTP Response : ")); DEBUG(status); DEBUG('\n');
      return false;
    } else {
      DEBUG(F("Success (")); DEBUG(status); DEBUG(F(")\n"));
    }

    while (!http.endOfHeadersReached()) {
      if (http.headerAvailable()) {
        const String name = http.readHeaderName();
        const String value = http.readHeaderValue();
        DEBUG(F("Header ")); DEBUG(name); DEBUG(':'); DEBUG(value); DEBUG('\n');
      }
    }
    const String body = http.responseBody();
    DEBUG(F("Body: ")); DEBUG(body); DEBUG('\n');
      
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
 /*
  String getParameters() {
    const String path = String(F("/device/GSM-")) + imei + F("/parameters");
    String response;
    if (http.get(path, response)) {
      Serial.println(response);
      return response;
    } else {
      Serial.println(F("Erreur de GET"));
      return String("");
    }
  }
*/  
 
private:
  static App* pApp;

  Sensors sensors;
  Parameters parameters;
//  Http http;

  const String GPRS_APN;
  const String GPRS_LOGIN;
  const String GPRS_PASSWORD;

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

/**
 * Structure des éléments enregistrés dans la file d'attente des mesures.
 */
  struct sample_t {
    uint32_t epoch;
    const __FlashStringHelper* variable;  // 
    float value;
  };

  static 
  QueueArray<sample_t> queue;

  static volatile
  bool fIntTimer;

  unsigned ranges[15];

};

App* App::pApp;
RTCZero App::rtc;
QueueArray<App::sample_t> App::queue;
volatile bool App::fIntTimer;
const int App::QUEUE_DEPTH = 10;


