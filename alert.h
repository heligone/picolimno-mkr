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

class Alert {

private:
  float fSeuil;
  float fEcart;

  bool fStatus;
  

protected:

public:
/**
 * Constructeur de l'alerte.
 * Une alerte change d'état si la valeur passe en dessous du seuil ou si la valeur passe au dessus du (seuil + écart).
 * 
 * @param seuil Le seuil de l'alerte (defaut 0).
 * @param l'écart autour du seuil (hystérésis), (defaut 0).
 */
  Alert(const float& seuil = 0, const float& ecart = 0) :
    fSeuil(seuil),
    fEcart(ecart),
    fStatus(false)
  {}

  Alert(const bool& sens) :
    fSeuil(0),
    fEcart(0),
    fStatus(sens)
  {}

  Alert& operator=(const Alert& aAlert) {
    fSeuil = aAlert.fSeuil;
    fEcart = aAlert.fEcart;
    // on ne modifie pas l'état !
  }

/**
 * Indique si l'alerte a changé d'état.
 * @warning Ne retourne pas l'état courant.
 * 
 * @param T Une référence de nouvelle valeur à tester.
 * @return true si l'état de l'alerte a changé, false sinon.
 */
  bool test(const float& value) {

DEBUG(__FUNCTION__);
DEBUG(fStatus);
DEBUG("->");
DEBUG(fSeuil);
DEBUG('?');
DEBUG(value);
    if (fStatus) {  // true 
      if (value < fSeuil ) {
        fStatus = false;
        return true; // changement
      }
    } else {
      if (value > fSeuil + fEcart ) {
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

  bool enabled() const {
    return (fSeuil != 0 || fEcart != 0);
  }
  
  
};


