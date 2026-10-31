#pragma once
#include <Arduino.h>

struct ConfigWifi {
  char ssid[33];       // 32 caracteres max (norme Wi-Fi) + '\0'
  char password[65];   // 64 caracteres max (WPA2) + '\0'
};
extern ConfigWifi cfgWifi;

bool monterStockage();     // rend LittleFS utilisable
void listerFichiers();     // le TODO du prof : "ls" de la flash
bool chargerConfig();      // /config.json -> Wi-Fi + seuils
bool sauverSeuils();       // seuils actuels -> /config.json