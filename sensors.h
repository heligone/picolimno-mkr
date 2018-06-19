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
 *  sensor.h
 *  Header of all sensors.
 *
 *  @author Marc Sibert
 *  @version 1.0 14/04/2018
 *  @Copyright 2018 Marc Sibert
 */

#pragma once

/**
 * Classe définissant les méthodes d'accès aux capteurs ainsi que de leur initilisation et celle du contrôleur.
 * La méthode begin() permet une intialisation tardive (lazy setup).
 * Le contructeur de fait qu'initialiser les variables d'instance.
 * @warning On ne DOIT pas instancier plusieurs fois cette classe ; c'est un syngleton.
 */
class Sensors {

private:
/// Ports
  const int mbTriggerPin, mbEchoPin, amDataPin;

protected:

  bool readAM2302(float& aTemp, float& aHygro) const {
    pinMode(amDataPin, OUTPUT);
    digitalWrite(amDataPin, LOW);   // down
    delayMicroseconds(1000);    // wait 1 ms
    pinMode(amDataPin, INPUT_PULLUP);
    
    const unsigned p = pulseIn(amDataPin, LOW, 150U);    
    if (p < 70 || p > 90) return false;   // should be about 80 ms or an error

    uint16_t hygro = 0;
    for (byte i = 16; i > 0; --i) {
      const unsigned p = pulseIn(amDataPin, HIGH, 150U);
      if (!p) return false;   // timeout
      hygro *= 2;
      if (p > 50) ++hygro;
    }

    uint16_t temp = 0;
    for (byte i = 16; i > 0; --i) {
      const unsigned p = pulseIn(amDataPin, HIGH, 150U);
      if (!p) return false;   // timeout
      temp *= 2;
      if (p > 50) ++temp;
    }

    uint8_t chk = 0;
    for (byte i = 8; i > 0; --i) {
      const unsigned p = pulseIn(amDataPin, HIGH, 150U);
      if (!p) return false;   // timeout
      chk *= 2;
      if (p > 50) ++chk;
    }

    if ( ((temp & 0xff00) / 256 + (temp & 0x00ff) + (hygro & 0xff00) / 256 + (hygro & 0x00ff) - chk) & 0x00ff ) return false;
  
    aHygro = hygro / 10.0f;
    aTemp = (temp & 0x8000 ? -1 : 1) * (temp & 0x7fff) / 10.0f;

    return true;
  }



public:
/**
 * Constructeur initialise les constantes matérielles.
 * Ne réalise aucune action.
 */
  Sensors(const int& mbTrigger, const int& mbEcho, const int& amData) :
    mbTriggerPin(mbTrigger),
    mbEchoPin(mbEcho),
    amDataPin(amData)
  {};

/**
 * Configure les ports et active les capteurs.
 * @return Le succès de l'initilisation, ou pas.
 */
  bool begin() {
    analogReadResolution(10);
  
    pinMode(mbEchoPin, INPUT);
    digitalWrite(mbTriggerPin, LOW);
    pinMode(mbTriggerPin, OUTPUT);

    pinMode(amDataPin, INPUT_PULLUP);

    return true;
  };

/**
 * Retourne la distance mesurée par le capteur Maxbotix MBxxxx.
 * "To calculate the distance, use a scale factor of 58uS per cm." 
 * ou pas !!! je comprends pas, mais la mesure est correcte.
 * 
 * @return La distance mesurée exprimée en mm ou 0 si la mesure est invalide (hors interval).
 */
  unsigned long sampleRange() const {
    digitalWrite(mbTriggerPin, HIGH);
    delayMicroseconds(100); 
    digitalWrite(mbTriggerPin, LOW);

    const unsigned long start = millis();
    const unsigned long pulse = pulseIn(mbEchoPin, HIGH, 170000UL); // attendre env. 148 ms (mesure et calcul)
    while (millis() - start < 170) ;  // attendre en tout 166ms avant la fin de toute la transmission
    return (pulse < 600) || (pulse > 9000) ? 0 : pulse; // Hors interval : retour 0 == mesure invalide
  }

/**
 * Lance un échantillonnage et indique à la fois la température et l'humidité mesurées.
 * @see https://cdn-shop.adafruit.com/datasheets/Digital+humidity+and+temperature+sensor+AM2302.pdf
 * @note Lance deux fois la mesure car la première et parfois suspecte.
 * 
 * @param aTemp Retourne la température mesurée en °C.
 * @param aHygro Retourne le taux d'humidité dans l'air en %H.
 * @return Le succès de la collecte des résultats, ou pas ; en cas d'échec, les paramètres précédents doivent être ignorés.
 */
  bool sampleAM2302(float& aTemp, float& aHygro) const {
    readAM2302(aTemp, aHygro);
    return readAM2302(aTemp, aHygro);
  }


/**
 * Mesure la tension de la batterie LiPo.
 * La valeur est une moyenne sur 10 échantillons successifs.
 * 
 * @return La tension en volts.
 */
  float sampleBattery() const {
    unsigned long a = 0;
    for (int i = 0; i < 10; ++i) {
      a += analogRead(ADC_BATTERY);
    }
    return(a * (3.3f * 153) / (1024 * 120.0f) / 10.0f);
  }
};

