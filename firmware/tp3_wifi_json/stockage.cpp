#include "stockage.h"
#include "config.h"
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

ConfigWifi cfgWifi;

bool monterStockage() {
  if (!LittleFS.begin(true)) {        // true : formate si la partition est vierge
    Serial.println("LittleFS : echec du montage");
    return false;
  }
  Serial.printf("LittleFS : %u / %u octets utilises\n",
                (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
  return true;
}

// Liste un dossier et ses sous-dossiers (recursion)
static void lister(const char* dossier, int indent) {
  File racine = LittleFS.open(dossier);
  File f = racine.openNextFile();
  while (f) {
    for (int i = 0; i < indent; i++) Serial.print("   ");
    if (f.isDirectory()) {
      Serial.printf("[DIR] %s/\n", f.name());
      lister(f.path(), indent + 1);
    } else {
      Serial.printf("[FIC] %-16s %5u octets\n", f.name(), (unsigned)f.size());
    }
    f = racine.openNextFile();
  }
}

void listerFichiers() {
  Serial.println("Fichiers en flash :");
  lister("/", 1);
}

bool chargerConfig() {
  // 1. valeurs par defaut (config.h) : l'objet fonctionne meme sans fichier
  strlcpy(cfgWifi.ssid,     WIFI_SSID,     sizeof(cfgWifi.ssid));
  strlcpy(cfgWifi.password, WIFI_PASSWORD, sizeof(cfgWifi.password));

  // 2. on essaie de lire le fichier
  File f = LittleFS.open("/config.json", "r");
  if (!f) {
    Serial.println("config.json absent : valeurs par defaut");
    return false;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);   // fichier -> objet
  f.close();
  if (err) {
    Serial.printf("config.json invalide (%s) : valeurs par defaut\n", err.c_str());
    return false;
  }

  // 3. on prend chaque valeur du fichier, ou on garde la valeur par defaut
  strlcpy(cfgWifi.ssid,     doc["wifi"]["ssid"]     | WIFI_SSID,     sizeof(cfgWifi.ssid));
  strlcpy(cfgWifi.password, doc["wifi"]["password"] | WIFI_PASSWORD, sizeof(cfgWifi.password));
  params.seuilBas  = doc["regul"]["lt"] | params.seuilBas;
  params.seuilHaut = doc["regul"]["ht"] | params.seuilHaut;

  Serial.printf("Config chargee : SSID=%s  SB=%.1f  SH=%.1f\n",
                cfgWifi.ssid, params.seuilBas, params.seuilHaut);
  return true;
}

bool sauverSeuils() {
  JsonDocument doc;
  File f = LittleFS.open("/config.json", "r");      // relire pour GARDER la partie wifi
  if (f) { deserializeJson(doc, f); f.close(); }

  doc["regul"]["lt"] = params.seuilBas;
  doc["regul"]["ht"] = params.seuilHaut;

  f = LittleFS.open("/config.json", "w");           // "w" : on ecrase
  if (!f) return false;
  serializeJsonPretty(doc, f);                      // objet -> fichier (lisible)
  f.close();
  Serial.println("Seuils sauvegardes dans /config.json");
  return true;
}