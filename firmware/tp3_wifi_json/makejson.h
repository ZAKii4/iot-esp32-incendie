/*
 * makejson.h - Modele JSON de l'ESP (impose par le prof, "pre defined JSON model")
 * ------------------------------------------------------------------------
 * 1. On remplit une structure C (esp_model) avec l'etat de l'objet.
 * 2. makeJSON_fromstatus() la transforme en document JSON (ArduinoJson).
 * 3. envoyerStatus() SERIALISE ce document : objet -> chaine de caracteres,
 *    ecrite sur une seule ligne du port serie.
 *
 * Bibliotheque : ArduinoJson (Benoit Blanchon), version 7
 */
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

typedef struct {
  // ---- "status" : valeurs vivantes, issues des capteurs/actionneurs
  float temperature;        // NAN si indisponible -> null dans le JSON
  int   luminosity;         // 0..4095
  bool  regulationRunning;  // "RUNNING" / "HALT"
  bool  fireDetected;
  bool  heaterState;        // "ON" / "OFF"  (radiateur)
  bool  coolerState;        // "ON" / "OFF"  (climatisation)
  int   fanSpeed;           // % de PWM (0..100) : pas de tachymetre sur ce ventilo

  // ---- "regul" : seuils de la regulation
  float lowThreshold;
  float highThreshold;

  // ---- "net"
  unsigned long uptimeS;
  char  mac[18];            // "AA:BB:CC:DD:EE:FF" + '\0'
} esp_model;

void      remplirIdentite(esp_model* em);   
const char* identifiantESP();
JsonDocument makeJSON_fromlocation(double lat, double lon);
JsonDocument makeJSON_fromstatus(const esp_model* em);
void      envoyerStatus(const esp_model* em);     // serialise sur le port serie
