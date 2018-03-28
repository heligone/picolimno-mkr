# picolimno-mkr

Télémètre à ultra-sons basé sur le carte Arduino MKRGSM-1400.

![Image of breadboard](proto.jpg)

## Compilation
Après avoir cloner le répertoire, penser à ajouter le fichier <code>secrets.h</code> qui devra contenir le code suivant :

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

