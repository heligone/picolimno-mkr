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
   @file
   Picolimno MKR V1.0 project
   gsm.h
   Purpose: Define GSM class to control the GSM device.

   @author Marc SIBERT
   @version 1.0 03/08/2018
*/

#pragma once

#define TINY_GSM_MODEM_UBLOX
// #define TINY_GSM_RX_BUFFER 512
// #define TINY_GSM_TX_BUFFER 512
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

// #define LOG 1
#ifdef LOG
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialGSM, Serial);
#endif



/**
   Classe GSM.
   Singleton permettant de gérer les états de connexion de la carte GSM au réseau GPRS.
*/
class Communication {

  public:
    /**
       Structure des éléments enregistrés dans la file d'attente des mesures.
    */
    struct sample_t {
      uint32_t epoch;
      const __FlashStringHelper* variable;  //
      float value;
    };

    /**
       Factory for singleton GSM.

       @return the only one (singleton) App instance.
    */
    static Communication& getInstance(const __FlashStringHelper aApn[], const __FlashStringHelper aLogin[], const __FlashStringHelper aPassword[], const __FlashStringHelper aServerName[], const int aServerPort) {
      if (!pCommunication) {
        pCommunication = new Communication(aApn, aLogin, aPassword, aServerName, aServerPort);
      }
      return *pCommunication;
    }

    /**
       Retourne l'IMEI de la carte.

       @return Une chaîne contenant l'IMEI.
    */
    String getIMEI() const {
      if (connectGSMGPRS(GSM_CONNECTION)) {
        return modem.getIMEI();
      } else {
        DEBUG(F("Error connecting GSM or GPRS in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG('\n');
        return "";
      }
    }

    /**
       Requète la liste des paramètres et place les réponses dans les paramètres transmis en référence.

       @param aIMEI Une chaîne contenant le numéro IMEI du device.
       @param aRtc Une structure contenant l'heure pour mise à jour à partir de la réponse.
       @return Une chaîne JSON contenant chaque paramètre et sa valeur.
    */
    bool getParameters(const String aIMEI, RTCZero& rtc, Alert& alert1, Alert& alert2, byte& startTime, byte& stopTime, int& reset) const {
      if (!connectGSMGPRS(GPRS_CONNECTION)) {
        DEBUG(F("No success connecting GPRS and getting parameters in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("!\n"));
        return false;
      }

      TinyGsmClient client(modem);
      
      const String server = serverName;
      HttpClient http = HttpClient(client, server, serverPort);
      http.sendHeader(F("Host"), server);
      http.sendHeader(F("Connection: close"));
      http.sendHeader(F("Accept: application/json;charset=utf-8"));

      const String path = String(F("/device/GSM-")) + aIMEI + F("/parameters");
      DEBUG("GET "); DEBUG(path); DEBUG('\n');

      int status;
      for (int i = 0; i < 3; ++i) {
        const int err = http.get(path);
        if (err != 0) {
          DEBUG(F("Error on GET (")); DEBUG(err); DEBUG(F(") in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("!\n"));
          delay(500);
          continue;
        }
        status = http.responseStatusCode();
        if (status <= 0) {
          DEBUG(F("Internal error on GET (")); DEBUG(status); DEBUG(F(") in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("!\n'"));
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
            strptime(value.c_str(), "%a, %e %h %Y %H:%M:%S %z", &tm);
            rtc.setTime(tm.tm_hour, tm.tm_min, tm.tm_sec);
            rtc.setDate(tm.tm_mday, tm.tm_mon + 1, tm.tm_year % 100);
          }
        }
      }

      const String body = http.responseBody();
      DEBUG(F("Body: ")); DEBUG(body); DEBUG('\n');
//      client.stop();

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
        const byte mm = s.substring(s.indexOf(':') + 1).toInt();
        reset = hh * 60U + mm;
      } else reset = -1;

      return true;
    }

    /**
       Transmet un échantillon sample_t sérialisé sous la forme JSON d'un tableau d'un seul élément.
       @param sample L'échantillon à traduire en JSON avant de le transmettre.
       @return Le succès de la transmission, ou pas.
    */
    bool sendSample(const sample_t& sample, const String& aIMEI) const {
      return sendSamples(&sample, 1, aIMEI);
    }

    /**
       Transmet plusieurs échantillons sample_t sérialisés sous la forme JSON d'un tableau d'éléments.
       @param samples Les échantillons à traduire en JSON avant de les transmettre. Tout le tableau est transmis.
       @return Le succès de la transmission, ou pas.
    */
    template<size_t N>
    bool sendSamples(const sample_t (&samples)[N], const String& aIMEI) const {
      return sendSamples(samples, N, aIMEI);
    }

    /**
       Transmet plusieurs échantillons sample_t sérialisés sous la forme JSON d'un tableau d'éléments.
       @param samples Les échantillons à traduire en JSON avant de les transmettre.
       @param n Le nombre d'échantillons du tableau à transmettre (premiers).
       @return Le succès de la transmission, ou pas.
    */
    bool sendSamples(const sample_t samples[], const size_t n, const String& aIMEI) const {
      if (!connectGSMGPRS(GPRS_CONNECTION)) {
        DEBUG(F("No success connecting GPRS and sending samples in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("!\n"));
        return false;
      }

      TinyGsmClient client(modem);
      
      const String server = serverName;
      HttpClient http = HttpClient(client, server, serverPort);
      http.sendHeader(F("Host"), server);
      http.sendHeader(F("Connection: close"));

      const String path = String(F("/device/GSM-")) + aIMEI + F("/samples");
      DEBUG("PUT "); DEBUG(path); DEBUG('\n');

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

      const String contentType(F("application/json"));

      int status;
      for (int i = 0; i < 3; ++i) {
        const int err = http.put(path, contentType, json);
        if (err != 0) {
          DEBUG(F("Error on PUT (")); DEBUG(err); DEBUG(F(") in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("!\n"));
          delay(500);
          continue;
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

//      client.stop();
      return true;
    }

    /**
       Transmet l'état du device sous la forme d'un flux Json qui contient le statut ainsi que d'autres éléments :
       - L'heure mémorisée au moment de la transmission ;
       - L'état tel que passé en paramètre ;
       - L'IP du périphérique ;

       @param aState L'état transmis dans le flux Json.
       @return Le succès de la transmission, ou pas.
    */
    bool sendStatus(RTCZero& aRTC, const String& aState, const String& aIMEI) const {
      if (!connectGSMGPRS(GPRS_CONNECTION)) {
        DEBUG(F("No success connecting GPRS and sending status in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("!\n"));
        return false;
      }

      TinyGsmClient client(modem);
      
      const String server = serverName;
      HttpClient http = HttpClient(client, server, serverPort);
      http.sendHeader(F("Host"), server);
      http.sendHeader(F("Connection: close"));

      const String path = String(F("/device/GSM-")) + aIMEI + F("/status");
      DEBUG("PUT "); DEBUG(path); DEBUG('\n');

      char buffer[25];
      snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ", 2000 + aRTC.getYear(), aRTC.getMonth(), aRTC.getDay(), aRTC.getHours(), aRTC.getMinutes(), aRTC.getSeconds() );
      String json(F("{\"timestamp\": \""));
      json += buffer;
      json += F("\",\"status\":\"");
      json += aState;
      json += F("\",\"IP\":\"");
      json += modem.getLocalIP();
      json += F("\"}");
      const String contentType(F("application/json"));

      int status;
      for (int i = 0; i < 3; ++i) {
        const int err = http.put(path, contentType, json);
        if (err != 0) {
          DEBUG(F("Error on PUT (")); DEBUG(err); DEBUG(F(") in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("!\n"));
          delay(500);
          continue;
        }
        status = http.responseStatusCode();
        if (status <= 0) {
          DEBUG(F("Internal error on PUT (")); DEBUG(status); DEBUG(F(") in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("!\n"));
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

//      client.stop();
      return true;
    }

    /**
       Initialisation des composants de communication.
    */
    void setup() {
      // set serial baudrate
      SerialGSM.begin(115200);
      // hard resert
      pinMode(GSM_RESETN, OUTPUT);
      digitalWrite(GSM_RESETN, HIGH);
      delay(100);
      digitalWrite(GSM_RESETN, LOW);
      delay(3000);

      if (!modem.restart()) {
        DEBUG(F("Error restarting modem!\n"));
      }

      const String info = modem.getModemInfo();
      DEBUG(F("- TinyGSM using ")); DEBUG(info); DEBUG('\n');

      const String SCCID = modem.getSimCCID();
      DEBUG(F("- Sim CCID ")); DEBUG(SCCID);
      const SimStatus ss = modem.getSimStatus();
      DEBUG(F(", Status ")); DEBUG(ss == 0 ? F("ERROR!") : ss == 1 ? F("READY.") : ss == 2 ? F("LOCKED!") : F("unknown!")); DEBUG('\n');

      const int battLevel = modem.getBattPercent();
      DEBUG("- Battery level: "); DEBUG(battLevel); DEBUG('\n');

    }

  protected:
    /**
       Etats possible de la connexion GSM.
    */
    enum state_t {
      IDLE,                 ///< Non connecté
      GSM_CONNECTION_ONLY,  ///< Connexion GSM only
      GSM_CONNECTION,       ///< Connexion GSM, possibly GPRS
      GPRS_CONNECTION       ///< Connexion GSM & GPRS
    };

    /**
       Constructeur de l'instance.
       Il est protégé pour n'être utilisé que par la Factory (singleton).

       @param aApn Une chaîne contenant la valeur de l'APN.
       @param aLogin Une chaîne contenant la valeur du compte de connexion.
       @param aPassword Une chaîne contenant la valeur du mot de passe de connexion.
    */
    Communication(const __FlashStringHelper aApn[], const __FlashStringHelper aLogin[], const __FlashStringHelper aPassword[], const __FlashStringHelper aServerName[], const int aServerPort) :
#ifdef LOG
      modem(debugger),
#else
      modem(SerialGSM),
#endif
      apnName(aApn),
      apnLogin(aLogin),
      apnPassword(aPassword),
      serverName(aServerName),
      serverPort(aServerPort)
    {}

    /**
       Connecte, déconnecte ou reconnecte le GSM et/ou le GPRS selon les besoins.

       @param state État demandé (IDLE, GSM_CONNECTION, GPRS_CONNECTION).
       @param retry Nombre de réessais pour obtenir l'état demandé, 10 par défaut.
       @return L'état de la connexion, true si ok, false en cas d'erreur.
    */
    bool connectGSMGPRS(const state_t aState, const byte aRetry = 10) const {
      DEBUG(F("Connect")); 
      //DEBUG(aState);
      DEBUG(F("..."));

    // Restart takes quite some time
    // To skip it, call init() instead of restart()
    // FOR THE MKR GSM 1400
    // https://github.com/vshymanskyy/TinyGSM/pull/161/commits/57170c7565846df19bc87e729ee07aa7477e0597
    // reset / powerup the modem
    pinMode(GSM_RESETN, OUTPUT);
    digitalWrite(GSM_RESETN, HIGH);
    delay(100);
    digitalWrite(GSM_RESETN, LOW);
  
    // Restart takes quite some time
    // To skip it, call init() instead of restart()
    DEBUG(F("Initializing modem...")); 
    modem.restart();
    String modemInfo = modem.getModemInfo();
    DEBUG(F("Modem: "));
    DEBUG(modemInfo); DEBUG(F("\n"));
    // Unlock your SIM card with a PIN
    //modem.simUnlock("1234");
  
    DEBUG(F("Waiting for GSM network..."));
    if (!modem.waitForNetwork()) {
      DEBUG(" GSM fail"); DEBUG(F("\n"));
      delay(10000);
      return 0;
    }
    DEBUG("GSM OK"); DEBUG(F("\n"));
  
    DEBUG(F("Connecting to "));
    DEBUG(APN_NAME); DEBUG(F("..."));
    if (!modem.gprsConnect(APN_NAME, APN_USERNAME, APN_PASSWORD)) {
      DEBUG(F("GPRS fail ! ")); DEBUG(F("\n"));
      delay(10000);
      return 0;
    }
    DEBUG("GPRS OK"); DEBUG(F("\n"));

    DEBUG(F("connected with Operator ")); DEBUG(modem.getOperator());
    DEBUG(F(",Signal ")); DEBUG(modem.getSignalQuality()); DEBUG('\n');

    /*
          switch (aState) {
            case IDLE :
              for (int i = 0; i < aRetry; ++i) {
                DEBUG(i + 1); DEBUG('/'); DEBUG(aRetry); DEBUG(',');
                if (modem.isGprsConnected()) {
                  if (!modem.gprsDisconnect()) {
                    DEBUG(F("Can not disconnect from GPRS to go to IDLE!\n"));
                    continue;
                  }
                }
                return true;
              }
              break;
            case GSM_CONNECTION_ONLY :
            case GSM_CONNECTION :
              for (int i = 0; i < aRetry; ++i) {
                DEBUG(i+1); DEBUG('/'); DEBUG(aRetry); DEBUG(',');
                if (modem.isNetworkConnected()) {
                  if (aState == GSM_CONNECTION_ONLY) {
                    if (modem.isGprsConnected() && modem.gprsDisconnect()) return true;
                  } else {
                    return true;
                  }
                } else {  // not yet connected
                  if (!modem.waitForNetwork()) {
                    DEBUG(F("Error in waitForNetwork()\n"));
                    delay(500);
                    continue;
                  }
                  if (!modem.isNetworkConnected()) {
                    DEBUG(F("Error in waitForNetwork()\n"));
                    delay(500);
                    continue;
                  }
                }
    
                DEBUG(F("connected with Operator ")); DEBUG(modem.getOperator());
                DEBUG(F(",Signal ")); DEBUG(modem.getSignalQuality()); DEBUG('\n');
    
                return true;
              }
              break;
            case GPRS_CONNECTION :
              for (int i = 0; i < aRetry; ++i) {
                DEBUG(i + 1); DEBUG('/'); DEBUG(aRetry); DEBUG(',');
                if (modem.isGprsConnected()) return true;
    
                if (!modem.isNetworkConnected()) {
                  if (!modem.waitForNetwork()) {
                    DEBUG(F("Error in waitForNetwork()!\n"));
                    delay(500);
                    continue;
                  }
                  if (!modem.isNetworkConnected()) {
                    DEBUG(F("Still not Network Connected!\n"));
                    delay(500);
                    continue;
                  }
                }   // connected now
    
                const String name(apnName);
                const String login(apnLogin);
                const String password(apnPassword);
    
                if (!modem.gprsConnect(name.c_str(), password.c_str())) {
                  DEBUG(F("Error in gprsConnect("));
                  DEBUG(name); DEBUG(','); DEBUG(login); DEBUG(','); DEBUG(password); DEBUG(F(")\n"));
                  delay(500);
                  continue;
                }
                if (!modem.isGprsConnected()) {
                  DEBUG(F("Still not GPRS connected!\n"));
                  delay(500);
                  continue;
                } // GPRS connected now
    
                return true;
              }
              break;
          }
          return false;
          */
    }

    /*

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

        DEBUG(F("GPRS (")); DEBUG(apn); DEBUG(F(")...\n"));
        bool ok;
        for (byte i = 0; i < retry; ++i) {
          ok = modem.gprsConnect(apn.c_str(), login.c_str(), password.c_str());
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
    */

  private:
    static Communication* pCommunication;

    mutable TinyGsm modem;    ///< Instance modifiable sans rompre le const de la méthode utilisant cet attribut.

    const __FlashStringHelper* apnName;
    const __FlashStringHelper* apnLogin;
    const __FlashStringHelper* apnPassword;
    const __FlashStringHelper* serverName;
    const int serverPort;
};

Communication* Communication::pCommunication;
