#pragma once
#include <Arduino.h>
#include "makejson.h"

struct CibleMqtt {
  char     broker[64];  // nom ou ip du broker
  uint16_t port;  // port du broker
  char     topic[65];  // topic de publication
  uint16_t sp;  // periode de publication en s (0 = arret)
};

void        demarrerMqtt();
CibleMqtt   cibleMqtt();
bool        demanderCibleMqtt(String broker, String port, String topic, String sp, String& erreur);
void        gererMqtt(const esp_model* em);
const char* etatMqtt();