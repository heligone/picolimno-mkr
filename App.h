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

#include "sensors.h"
#include "parameters.h"
#include "alert.h"

#define TINY_GSM_MODEM_UBLOX
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

// Temps en secondes entre deux mesures de distance
#define INTERVAL_MESURES 10

// Indique la méthode utilisée pour mettre le Pico à l'heure, ou rien :
// pas de define : pas de mise à l'heure,
// 0 : GSM,
// 1 : NTP.
#define GSM_NTP 1

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

// set serial baudrate
    SerialGSM.begin(115200);
// hard resert
    pinMode(GSM_RESETN, OUTPUT);
    digitalWrite(GSM_RESETN, HIGH);
    delay(100);
    digitalWrite(GSM_RESETN, LOW);

    modem.restart();
    const String info = modem.getModemInfo();
    DEBUG(F("TinyGSM using ")); DEBUG(info); DEBUG('\n');

// Connection GSM & GPRS    
    DEBUG(F("Setting up GSM connection... "));
    int err;
    for (byte i = 0; i < 10; ++i) {
      err = modem.waitForNetwork();
      if (!err) {
        DEBUG(err);
        DEBUG(' '); delay(500);
      } else break;
    }        
    if (!err) {
      DEBUG(F("Timeout! No GSM found!\n"));
      return false;
    } else {
      DEBUG(F("Connected.\n"));
    }

    DEBUG(F("Getting IMEI... "));
    imei = modem.getIMEI();
    DEBUG(F("DeviceID: GSM-")); DEBUG(imei); DEBUG('\n');

    DEBUG(F("Setting up GPRS connection... "));
    for (byte i = 0; i < 10; ++i) {
      err = modem.gprsConnect(String(GPRS_APN).c_str(), String(GPRS_LOGIN).c_str(), String(GPRS_PASSWORD).c_str());
      if (!err) {
        DEBUG(err); DEBUG(' ');
        delay(500);
      } else break;
    }        
    if (!err) {
      DEBUG(F("Timeout! No GPRS network found!\n"));
      return false;
    } else {
      DEBUG(F("Connected.\n"));
    }

// Mise à l'heure
    rtc.begin();
#if (GSM_NTP == 0)
// Get GSM Network date
    DEBUG(F("Requesting GSM date... "));

// Si le compilateur plante sur la ligne suivante, il faudrait mettre à jour le source de TinyGSM sur la base de cette proposition :
// https://github.com/vshymanskyy/TinyGSM/pull/165/commits/712eaea3eb1f6b8c55747f0863a8ee3478991318    
    const String dt = modem.getGSMDateTime(DATE_FULL);
    DEBUG(dt); DEBUG('\n');

    const unsigned year = dt.substring(0,2).toInt();
    const byte mon = dt.substring(3,5).toInt();
    const byte mday = dt.substring(6,8).toInt();
    if ((2000 + year) >= ((__DATE__[7] - 0x30) * 1000 + (__DATE__[8] - 0x30) * 100 + (__DATE__[9] - 0x30) * 10 + (__DATE__[10] - 0x30))) { // année cohérante avec année de compilation
      const byte hour = dt.substring(9,11).toInt();
      const byte min = dt.substring(12,14).toInt();
      const byte sec = dt.substring(15,17).toInt();
      rtc.setTime((hour + 22) % 24, min, sec);
      rtc.setDate(mday, mon, year);
      DEBUG(getTimestamp());
      DEBUG(F(" (UTC) - Succeded.\n"));
    } else {
      DEBUG(F("Warning no GSM Network date!\n"));
      return false;
    }
#elif (GSM_NTP == 1)
// Get Date NTP    
    DEBUG(F("Requesting NTP date... "));
    uint32_t epoch;
    for (int i = 0; i < 10; ++i) {
      epoch = getNTP();
      if (epoch > 0) {
        delay(500);
        break;
      }
    }
    struct tm *const stm = gmtime((long*)&epoch);

    if ((1900 + stm->tm_year) >= ((__DATE__[7] - 0x30) * 1000 + (__DATE__[8] - 0x30) * 100 + (__DATE__[9] - 0x30) * 10 + (__DATE__[10] - 0x30))) { // année cohérante avec année de compilation
      rtc.setTime(stm->tm_hour, stm->tm_min, stm->tm_sec);
      rtc.setDate(stm->tm_mday, stm->tm_mon + 1, stm->tm_year % 100);
      DEBUG(stm->tm_mday); DEBUG('/'); DEBUG(stm->tm_mon); DEBUG('/'); DEBUG(stm->tm_year); DEBUG('\n');
      DEBUG(getTimestamp());
      DEBUG(F(" (UTC) - Succeded.\n"));
    } else {
      DEBUG(F("Warning no NTP date!\n"));
      return false;
    }
#endif    

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

#define RANGE_SEQ_LENGTH 25
      unsigned d[RANGE_SEQ_LENGTH];
      unsigned n = 0; // nb échantillons valides
      for (unsigned i = 0; i < RANGE_SEQ_LENGTH; ++i) {
        const unsigned s = sensors.sampleRange();
        if (s > 0) {
          d[n++] = sensors.sampleRange();
        }
      }
      
      qsort(d, n, sizeof(unsigned), [](const void* a, const void* b) -> int { 
        const unsigned int_a = * ( (unsigned*) a );
        const unsigned int_b = * ( (unsigned*) b );
        return (int_a > int_b) - (int_a < int_b);
      });
      
      const unsigned distance = (n > 0 ? d[n % 2 ? (n / 2) + 1 : n / 2] : 0);
      DEBUG(F("Distance : ")); DEBUG(distance / 10.0f); DEBUG(F(" - Ech. : ")); DEBUG(n); DEBUG('\n');



      
  
// Toutes les 15 min et dans les 10 premières secondes seulement
      if ((minu % 15 == 0) && (sec < 10)) {
        sample_t samples[4];
        size_t s = 0;
        
        samples[s] = { rtc.getEpoch(), F("range"), distance / 10.0f };  // Utilisation du dernier échantillon (valide).
#ifdef PETITES_TRAMES
        sendSample(samples[s]);
#endif
        ++s;

        float temp, hygro = 0.0;
        if (sensors.sampleAM2302(temp, hygro)) {
          samples[s] = { rtc.getEpoch(), F("temp"), temp };
#ifdef PETITES_TRAMES
          sendSample(samples[s]);
#endif
          ++s;
          DEBUG(F("Temperature : ")); DEBUG(temp); DEBUG('\n');
          
          samples[s] = { rtc.getEpoch(), F("hygro"), hygro };
#ifdef PETITES_TRAMES
          sendSample(samples[s]);
#endif
          ++s;
          DEBUG(F("Hygrometrie : ")); DEBUG(hygro); DEBUG('\n');
        }
        const float vBat = sensors.sampleBattery();
        samples[s] = { rtc.getEpoch(), F("vbat"), vBat };
#ifdef PETITES_TRAMES
        sendSample(samples[s]);
#endif
        ++s;
        DEBUG("Batterie : "); DEBUG(vBat); DEBUG('\n');

#ifndef PETITES_TRAMES
        sendSamples(samples, s);
#endif
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
    GPRS_PASSWORD(password),
    modem(SerialGSM)
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

    modem.sendAT(GF("+USOCR=17"));
    String socket;
    if (modem.waitResponse(1000L, socket) == 1) {
      socket.replace("+USOCR:", "");
      socket.replace(GSM_NL "OK" GSM_NL, "");
      socket.replace(GSM_NL, " ");
    }
    const String ip("195.154.107.205");
    const String len(sizeof(packetBuffer));
    modem.sendAT(GF("+USOST=") + socket + ',' + '"' + ip + GF("\",123,") + len);
    SerialGSM.readStringUntil('@');
    SerialGSM.write((char*)&packetBuffer, sizeof(packetBuffer));

    String rep;
    if (modem.waitResponse(1000L, rep) == 1) {

      modem.sendAT(GF("+USORF=") + socket + ',' + len);
      if (modem.waitResponse(1000L, rep) == 1) {
        rep.replace("+USORF:", "");
        rep.replace(GSM_NL "OK" GSM_NL, "");
        rep.replace(GSM_NL, " ");
        const int ipInd = rep.indexOf(',');
        const int portInd = rep.indexOf(',', ipInd+1);
        const int sizeInd = rep.indexOf(',', portInd+1);
        const int dataInd = rep.indexOf(',', sizeInd+1);
        const unsigned s = rep.substring(sizeInd+1, dataInd).toInt();
        if (s != sizeof(packetBuffer)) {
          DEBUG("Invalide NTP response length!");
          return 0;
        }
        memcpy(&packetBuffer, rep.c_str() + (dataInd + 2), s);
        return ((packetBuffer.txTm_s[0] * 0x100UL + packetBuffer.txTm_s[1]) * 0x100UL + packetBuffer.txTm_s[2]) * 0x100UL + packetBuffer.txTm_s[3] - 2208988800UL;
      } else {
        DEBUG("NTP Response Error!");
        return 0;
      }
    } else {
      DEBUG("NTP no response!");
      return 0;
    }
        
/*    
    Udp.begin(2390); // listening 

// Résoudre fr.pool.ntp.org ou utiliser une IP  
    const IPAddress timeServer(195,154,107,205);

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
*/
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

    const String serverName = String(F("api.picolimno.fr"));
    const unsigned serverPort = 80;

    TinyGsmClient client(modem);
    if (!client.connect(serverName.c_str(), serverPort)) {
      DEBUG(F("No connection !"));
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

    const String serverName = String(F("api.picolimno.fr"));
    const unsigned serverPort = 80;

    TinyGsmClient client(modem);;
    if (!client.connect(serverName.c_str(), serverPort)) {
      DEBUG(F("No connection !"));
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

  const __FlashStringHelper* GPRS_APN;
  const __FlashStringHelper* GPRS_LOGIN;
  const __FlashStringHelper* GPRS_PASSWORD;

  TinyGsm modem;
  
//  GPRS gprs;
//  GSM gsmAccess;

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

//  static 
//  QueueArray<sample_t> queue;

  static volatile
  bool fIntTimer;

  unsigned ranges[15];

};

App* App::pApp;
RTCZero App::rtc;
// QueueArray<App::sample_t> App::queue;
volatile bool App::fIntTimer;
const int App::QUEUE_DEPTH = 10;


