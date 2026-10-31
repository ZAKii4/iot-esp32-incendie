#pragma once
#include <Arduino.h>
#include "makejson.h"

struct CibleReporting {
  char     ip[16];
  uint16_t port;
  uint16_t sp;
};

CibleReporting cibleReporting();
bool demanderCible(String ip, String port, String sp, String& erreur);
void gererReporting(const esp_model* em);