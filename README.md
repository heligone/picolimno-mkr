# picolimno-mkr

Télémètre à ultra-sons basé sur le carte Arduino MKRZERO et modem ADAFRUIT  FONA - Mini Cellular GSM Breakout uFL Version (1946)

![Image of breadboard](doc/electroniquepico2019.jpg)

## Compilation
Après avoir cloner le répertoire, penser à ajouter le fichier
<code>secrets.h</code> qui devra contenir le code suivant :

```
/**
    Water-Level-MKRGSM1400 V1.0 project
    secrets.h
    Purpose: Define secrets for MKRGSM functions and others...
  
    @author Marc SIBERT
    @version 1.0 25/02/2018
    @warning *** This file shouldn't be uploaded in repo (Git) ***
*/
#pragma once

#define PIN_CODE ""

#define APN_NAME ""
#define APN_USERNAME ""
#define APN_PASSWORD ""
```

### Dépendances
* QueueArray : 
  <code>Croquis > Inclure une biliothèque >  Ajouter la bibliothèque .ZIP...</code> Importer préalablement le fichier disponible sur https://playground.arduino.cc/Code/QueueArray  ;
* MKRGSM :
  <code>Croquis > Inclure une biliothèque > MKRGSM</code> ;
* RTCZero :
  <code>Croquis > Inclure une biliothèque > RTCZero</code>
* ArduinoJson (https://github.com/bblanchon/ArduinoJson) :
  <code>Croquis > Inclure une biliothèque > Gérer les biliothèques</code> ; Ajouter "ArduinoJSON" dans le filtre et cliquer sur Installer.

## Matériel
Le montage est simple, il consiste simplement à connecter les 3 périphériques
sur la carte. On peut le faire soit en "volant" en utilisant le bornier 
supérieur, ou mieux, fixer le MKR GSM 1400 sur une plaque à pastilles, soit en 
le soudant directement, ou mieux (encore), en soudant deux rangées de barettes
de support.

* Batterie 3.7v :
  * Sur le connecteur J7 (en bas)

* Panneau Solaire :
  * Masse -> GND (MKR)
  * VCC -> VIN (MKR)

* Capteur AM2302 température & humidité ambiantes :
  * Rouge (VCC) -> VCC (MKR)
  * Jaune (bus data) -> 0 (MKR) - rangée du bas
  * Noir (GND) -> GND (MKR)

* Capteur de distance Maxbotix MB7x6x :
  * [1] [carré] (Non-Connecté)
  * (2) Echo -> 3 (MKR)
  * (3) Analog (NC)
  * (4) Trigger -> 2 (MKR)
  * (5) TX Serial (NC)
  * (6) VCC -> VCC (MKR)
  * (7) GND -> GND (MKR)

Le port USB peut être utilisé sans danger en même temps que le panneau solaire, 
sans risque de retour vers le PC, car l'alimentation assure cette protection.

![Vue de dessus](doc/20180411_143135.jpg)
![Vue de dessous](doc/20180411_143151.jpg)
![Vue du Capteur](doc/20180411_143222.jpg)
