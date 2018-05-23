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
 *  alert.h
 *  Define an Alert class.
 *
 *  @author Marc Sibert
 *  @version 1.0 23/5/2018
 *  @Copyright 2018 Marc Sibert
 */

#pragma once

template<typename T>
class Alert {

private:
  const T fSeuil;
  const T fEcart;

  bool fStatus;
  

protected:

public:
/**
 * Constructeur de l'alerte.
 * Une alerte change d'état si la valeur dépasse le seuil + l'écart/2 ou si la valeur passe en dessous du seuil - écart / 2.
 * 
 * @param seuil Le seuil de l'alerte.
 * @param l'écart autour du seuil (hystérésis).
 */
  Alert(const T& seuil, const T& ecart) :
    fSeuil(seuil),
    fEcart(ecart),
    fStatus(false)
  {}

/**
 * Indique si l'alerte a changé d'état.
 * @warning Ne retourne pas l'état courant.
 * 
 * @param T Une référence de nouvelle valeur à tester.
 * @return true si l'état de l'alerte a changé, false sinon.
 */
  bool test(const T& value) {
    if (fStatus) {  // true 
      if (value < fSeuil - fEcart / 2) {
        fStatus = false;
        return true; // changement
      }
    } else {
      if (value > fSeuil + fEcart / 2) {
        fStatus = true;
        return true;
      }
    }
    return false; // pas de changement d'état
  }

/**
 * Retourne le statut de l'alerte.
 * Le statut indique si la dernière valeur testée est supérieure ou inférieure au seuil, à l'hystérésis près.
 * 
 * @return true si dépassement par valeur supérieure, false par dépassement par valeur inférieure.
 */
  bool status() const {
    return fStatus;
  }
  
  
};


