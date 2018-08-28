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

#include <MKRGSM.h>
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
        DEBUG(F("Error connecting GSM in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG('\n');
        return "";
      }
    }

    /**
       Requète la liste des paramètres et place les réponses dans les paramètres transmis en référence.

       @param aIMEI Une chaîne contenant le numéro IMEI du device.
       @param aRtc Une structure contenant l'heure pour mise à jour à partir de la réponse.
       @return Une chaîne JSON contenant chaque paramètre et sa valeur.
    */
    bool getParameters(const String& aIMEI, RTCZero& rtc, Alert& alert1, Alert& alert2, byte& startTime, byte& stopTime, int& reset) const {
      if (!connectGSMGPRS(GPRS_CONNECTION)) {
        DEBUG(F("No success connection GPRS in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("!\n"));
        return false;
      }

      GSMClient client;
      
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
        DEBUG(F("No success connection GPRS in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("!\n"));
        return false;
      }

      GSMClient client;
      
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

      client.stop();
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
        DEBUG(F("No success connection GPRS in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("!\n"));
        return false;
      }

      GSMClient client;
      
      const String server = serverName;
      HttpClient http = HttpClient(client, server, serverPort);
      http.sendHeader(F("Host"), server);
      http.sendHeader(F("Connection: close"));

      const String path = String(F("/device/GSM-")) + aIMEI + F("/status");
      DEBUG("PUT "); DEBUG(path); DEBUG('\n');

      char buffer[25];
      snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ", 2000 + aRTC.getYear(), aRTC.getMonth(), aRTC.getDay(), aRTC.getHours(), aRTC.getMinutes(), aRTC.getSeconds() );
      const IPAddress localIP = gprs.getIPAddress();
      String json(F("{\"timestamp\": \""));
      json += buffer;
      json += F("\",\"status\":\"");
      json += aState;
      json += F("\",\"IP\":\"");
      json += localIP;
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

      client.stop();
      return true;
    }

/**
 * Initialisation des composants de communication (modem UBlocs)
 */
    void setup() {
/// Tout est fait dans modem.begin()
      DEBUG(F("Starting modem... "));
      const int err = modem.begin();
      if (1 != err) {
        DEBUG(F("Error ")); DEBUG(err); DEBUG(F(" starting modem in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F("! Try to continue...\n"));
      } else {
        DEBUG(F("Ok.\n"));
      }
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
      modem(),
      gsm(),
      gprs(),
      
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
      DEBUG(F("Connect ")); DEBUG(aState); DEBUG(F("..."));
      switch (aState) {
        case IDLE :
          for (byte i = 1; i <= aRetry; ++i) {
            DEBUG(i); DEBUG('/'); DEBUG(aRetry); DEBUG(',');
            if (GPRS_READY == gprs.status()) {
              const GSM3_NetworkStatus_t st = gprs.detachGPRS();
              if ((GSM3_NetworkStatus_t::GSM_READY != st) || (GSM3_NetworkStatus_t::IDLE != st)) {
                DEBUG(F("Can not disconnect from GPRS in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F(", retry!\n"));
                delay(500);
                continue;
              }
            }
            if (1 == gsm.isAccessAlive()) {
              if (1 != gsm.lowPowerMode()) {
                DEBUG(F("Can not turn \"low power mode\" on in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F(", retry!\n"));
                delay(500);
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
            if ((GPRS_READY == gprs.status()) && (aState == GSM_CONNECTION_ONLY)) { // need GPRS deconnection ?
              const GSM3_NetworkStatus_t st = gprs.detachGPRS();
              if ((GSM3_NetworkStatus_t::GSM_READY != st) || (GSM3_NetworkStatus_t::IDLE != st)) {
                DEBUG(F("Can not disconnect from GPRS in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F(", retry!\n"));
                delay(500);
                continue;
              }
            }
            
            if (1 != gsm.isAccessAlive()) {  // not yet connected
              if (1 != gsm.noLowPowerMode()) {
                DEBUG(F("Can not turn \"low power mode\" off in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F(", retry!\n"));
                delay(500);
                continue;
              }
              DEBUG("try begin\n");
              gsm.begin(NULL, true, false);
              while ((0 == gsm.ready()) || (0 == gsm.isAccessAlive())) {
                const int r = gsm.ready();
                const int s = gsm.isAccessAlive();
                DEBUG("Waiting : "); DEBUG(r); DEBUG(','); DEBUG(s); DEBUG("\n");
                delay(1000);
              }
/*
              DEBUG("Begin return : "); DEBUG(st); DEBUG("\n");
              if (ERROR == st) {
                DEBUG(F("Can not begin GMS in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F(", retry!\n"));
                delay(500);
                continue;
              }
*/
              if (1 != gsm.isAccessAlive()) {
                DEBUG(F("GMS is not alive in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F(", retry!\n"));
                delay(500);
                continue;
              } 
            }
            GSMScanner scanner;
            const GSM3_NetworkStatus_t st = scanner.begin();
            if (ERROR == st) {
                DEBUG(F("Can not begin GSMScanner in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F(", continue!\n"));
                return true;
            }
            DEBUG(F("connected with Operator ")); DEBUG(scanner.getCurrentCarrier());
            DEBUG(F(",Signal ")); DEBUG(scanner.getSignalStrength()); DEBUG('\n');

            return true;
          }
          break;
        case GPRS_CONNECTION :
          for (int i = 0; i < aRetry; ++i) {
            DEBUG(i + 1); DEBUG('/'); DEBUG(aRetry); DEBUG(',');
            if (GPRS_READY == gprs.status()) return true;

            if (1 != gsm.isAccessAlive()) {  // not yet connected
              if (1 != gsm.noLowPowerMode()) {
                DEBUG(F("Can not turn \"low power mode\" off in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F(", retry!\n"));
                delay(500);
                continue;
              }
              const GSM3_NetworkStatus_t st = gsm.begin();
              if (ERROR == st) {
                DEBUG(F("Can not begin GMS in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F(", retry!\n"));
                delay(500);
                continue;
              }
              if (1 != gsm.isAccessAlive()) {
                DEBUG(F("GMS is not alive in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F(", retry!\n"));
                delay(500);
                continue;
              } 
            }

            const String name(apnName);
            const String login(apnLogin);
            const String password(apnPassword);

            const GSM3_NetworkStatus_t st = gprs.attachGPRS(name.c_str(), login.c_str(), password.c_str());
            if (GPRS_READY != st) {
              DEBUG(F("Can not attachGPR("));
              DEBUG(name); DEBUG(','); DEBUG(login); DEBUG(','); DEBUG(password); DEBUG(F(") in ")); DEBUG(F(__PRETTY_FUNCTION__)); DEBUG(F(", retry!\n"));
              delay(500);
              continue;
            } else {
              const GSM3_NetworkStatus_t st = gprs.status();
              if (GPRS_READY != st) {
                DEBUG(F("Still not GPRS connected!\n"));
                delay(500);
                continue;
              }
            } // GPRS connected now

            return true;
          }
          break;
      }
      return false;
    }

  private:
    static Communication* pCommunication;

    mutable GSMModem modem;     ///< Instance modifiable sans rompre le const de la méthode utilisant cet attribut.
    mutable GSM gsm;
    mutable GPRS gprs;

    const __FlashStringHelper* apnName;
    const __FlashStringHelper* apnLogin;
    const __FlashStringHelper* apnPassword;
    const __FlashStringHelper* serverName;
    const int serverPort;
};

Communication* Communication::pCommunication;

