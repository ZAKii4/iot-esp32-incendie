#include "capteurs.h"
#include "config.h"
#include <OneWire.h>
#include <DallasTemperature.h>

static OneWire           oneWire(PIN_TEMP);
static DallasTemperature capteurT(&oneWire);

static SourceTemp source       = SourceTemp::ABSENTE;
static int        lumReference = 0;

// ------------------------------------------------------------ temperature
void capteursInit() {
  analogReadResolution(12);                 // ESP32 : 0..4095

  capteurT.begin();
  if (capteurT.getDeviceCount() == 0)
    Serial.println("ATTENTION : aucun DS18B20 sur GPIO 23 -> tape 'T <val>' pour simuler");
  capteurT.setResolution(12);
  capteurT.setWaitForConversion(false);     // mode non bloquant
  capteurT.requestTemperatures();           // lance la 1re conversion

  calibrerLumiere();
}

float lireTemperature() {
  if (!isnan(params.tSimulee)) {            // mode simulation
    source = SourceTemp::SIMULEE;
    return params.tSimulee;
  }
  float t = capteurT.getTempCByIndex(0);    // resultat de la conversion precedente
  capteurT.requestTemperatures();           // on relance la suivante

  if (t == DEVICE_DISCONNECTED_C) {         // -127 : capteur absent ou HS
    source = SourceTemp::ABSENTE;
    return NAN;
  }
  source = SourceTemp::CAPTEUR;
  return t;
}

SourceTemp sourceTemperature() { return source; }

// ------------------------------------------------------------ lumiere
static int echantillonLumiere() {
  int v = analogRead(PIN_LUM);
  return LUM_INVERSEE ? 4095 - v : v;       // corrige un module cable "a l'envers"
}

int lireLumiere() {
  long somme = 0;
  for (int i = 0; i < 8; i++) somme += echantillonLumiere();  // moyenne anti-bruit
  return somme / 8;
}

void calibrerLumiere() {
  Serial.println("Calibration lumiere... ne mets RIEN devant le capteur");
  long somme = 0;
  for (int i = 0; i < 20; i++) { somme += echantillonLumiere(); delay(50); }
  lumReference = somme / 20;
  Serial.printf("Reference lumiere = %d\n", lumReference);
}

int referenceLumiere() { return lumReference; }
