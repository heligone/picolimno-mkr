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
 *  http.h
 *  HTTP/HTTPS client verbs.
 *
 *  @author Marc Sibert
 *  @version 1.0 15/04/2018
 *  @Copyright 2018 Marc Sibert
 */

#pragma once

/**
 * Cette classe implémente les méthodes nécessaires pour assurer la communication HTTPS via GPRS
 */
class Http {
private:
  const String host;
  const unsigned port;
  

protected:


public:
/**
 * Constructeur ne précisant que le nom du serveur (Host).
 * 
 * @param aServer[] Une chaîne de caractères contenant le nom du serveur.
 */
  Http(const __FlashStringHelper aServer[]) :
    Http(aServer, 80)
  {}

/**
 * Constructeur précisant le nom du serveur (Host) et le port TCP à utiliser.
 * 
 * @param aServer[] Une chaîne de caractères contenant le nom du serveur.
 * @param aPort Un entier indiquant le numéro du port TCP à utiliser.
 */
  Http(const __FlashStringHelper aServer[], const unsigned aPort) :
    host(aServer),
    port(aPort)
  {}

/**
 * Produit et transmet une requete HTTPS / GET avec le chemin préciser. La réponse est retournée en cas de succès seulement.
 * 
 * @param aPath Chemin inclus dans la requête.
 * @param aResponse Référence vers une chaîne de caractère pouvant recevoir la réponse (Body).
 * @return Un boolean indiquant le succès de la requête (HTTP ERR <= 300).
 */
  bool get(const String& aPath, String& aResponse) const {
    GSMClient client;

    if (!client.connect(host.c_str(), 80)) {
      Serial.println(F("Erreur de connexion"));
      return false;
    }
    
    client.print(F("GET "));   client.print(aPath);   client.println(F(" HTTP/1.0"));
    client.print(F("Host: ")); client.println(host);
    client.println(F("Content-Type: application/json"));
    client.println(F("Connection: close"));
    client.println();

    String s;
    const unsigned long start = millis();
    while ((client.available() || client.connected()) && (millis() - start) < 30000) {
      if (client.available() > 0) {
        const int c = client.read();
        if (c != -1) s += char(c);
      }
    }
    client.stop();
//    Serial.println(s);

    // Analyse : "HTTP/1.x err texte\n"
    const int a = s.indexOf('\r');  // Prochain CR
    if ((a >= 0) and (s.substring(0, 7).equalsIgnoreCase(F("HTTP/1.")))) {
      const unsigned err = s.substring(9).toInt();
      const int b = s.substring(9).indexOf(' ');  // prochain espace après "err"
      const String mess = s.substring(b+1, a);
      if ((b >= 0) && (err < 300)) {
        s = s.substring(a + 2); // first header
        bool json = false;
        bool update = false;
        int c;
    // Parcours de chaque header
        while ((c = s.indexOf('\r')) > 0) {
//Serial.println(">" + s.substring(0, c) + "<");
          const int d = s.indexOf(':');
          if (s.substring(0, d).equalsIgnoreCase(F("Content-Type")) and s.substring(d + 2, c).equalsIgnoreCase(F("application/json;charset=utf-8"))) {
            json = true;
          } else if (s.substring(0, d).equalsIgnoreCase(F("X-Parameters")) and s.substring(d + 2, c).equalsIgnoreCase(F("updated"))) {
            update = true;
          }
          s = s.substring(c + 2); // Next header
        }
        s = s.substring(c + 2); // Ligne vide avant body.

        aResponse = s;
        return true;
        
      } else {
Serial.print(F("Error "));
Serial.print(err);
Serial.println(mess);
      }
    } else {
      Serial.println(F("Pas de CR\n"));
    }
    Serial.println(F("Reponse HTTP invalide :\n"));
    Serial.println(s);
    return false;    
  }

/**
 * Émet une requête HTTP PUT vers le serveur de référence.
 * 
 * @param aPath Chemin transmis dans la requête.
 * @param aBody Contenu de la requête.
 * @return Le succès de la transmission.
 */
  bool put(const String& aPath, const String& aBody) const {
    DEBUG("PUT "); DEBUG(aPath); DEBUG('\n');
    
//    GSMSSLClient client;
    GSMClient client;

    if (!client.connect(host.c_str(), 80)) {
      Serial.println(F("Erreur de connexion"));
      return false;
    }
    
    client.print(F("PUT "));   client.print(aPath);   client.println(F(" HTTP/1.0"));
    client.print(F("Host: ")); client.println(host);
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

    String s;
    const unsigned long start = millis();
    while ((client.available() || client.connected()) && (millis() - start) < 30000) {
      if (client.available() > 0) {
        const int c = client.read();
        if (c != -1) s += char(c);
      }
    }
    client.stop();
    Serial.println(s);
    return true;
  }
  
};

