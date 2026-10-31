#include "makejson.h"
#include "config.h"
#include <WiFi.h>
#include "reporting.h"

static char identifiant[24];     // "ESP32 CDCF8C" (fin de la MAC = unique par carte)

// ------------------------------------------------------------ identite
// La MAC est gravee dans l'eFuse de la puce : on la lit sans activer le Wi-Fi.
void remplirIdentite(esp_model* em) {
  uint64_t m = ESP.getEfuseMac();               // octets dans l'ordre inverse
  uint8_t  b[6];
  for (int i = 0; i < 6; i++) b[i] = (m >> (8 * i)) & 0xFF;

  snprintf(em->mac, sizeof(em->mac), "%02X:%02X:%02X:%02X:%02X:%02X",
           b[0], b[1], b[2], b[3], b[4], b[5]);
  snprintf(identifiant, sizeof(identifiant), "ESP32 %02X%02X%02X", b[3], b[4], b[5]);
}
const char* identifiantESP() { return identifiant; }
// ------------------------------------------------------------ sous-objet "gps"
JsonDocument makeJSON_fromlocation(double lat, double lon) {
  JsonDocument doc;
  doc["lat"] = lat;
  doc["lon"] = lon;
  return doc;
}

// ------------------------------------------------------------ document complet
JsonDocument makeJSON_fromstatus(const esp_model* em) {
  JsonDocument doc;                             // v7 : taille geree automatiquement

  // "status" : JsonObject imbrique dans le document
  JsonObject status = doc["status"].to<JsonObject>();
  if (isnan(em->temperature)) status["temperature"] = nullptr;   // -> null
  else                        status["temperature"] = em->temperature;
  status["light"]    = em->luminosity;
  status["regul"]    = em->regulationRunning ? "RUNNING" : "HALT";
  status["fire"]     = em->fireDetected;                         // -> true / false
  status["heat"]     = em->heaterState ? "ON" : "OFF";
  status["cold"]     = em->coolerState ? "ON" : "OFF";
  status["fanspeed"] = em->fanSpeed;

  JsonObject location = doc["location"].to<JsonObject>();
  location["room"]    = LOC_ROOM;
  location["gps"]     = makeJSON_fromlocation(LOC_LAT, LOC_LON);   // objet dans l'objet
  location["address"] = LOC_ADDRESS;

  JsonObject regul = doc["regul"].to<JsonObject>();
  regul["lt"] = em->lowThreshold;
  regul["ht"] = em->highThreshold;

  JsonObject info = doc["info"].to<JsonObject>();
  info["ident"] = identifiant;
  info["user"]  = INFO_USER;
  info["loc"]   = INFO_LOC;

  char uptime[12];                              // char[] plutot que String (conseil du prof)
  snprintf(uptime, sizeof(uptime), "%lu", em->uptimeS);

  JsonObject net = doc["net"].to<JsonObject>();
  net["uptime"] = uptime;                       // chaine, comme dans le modele du prof
  net["ssid"]   = WiFi.isConnected() ? WiFi.SSID() : String("NOP");
  net["mac"]    = em->mac;
  net["ip"]     = WiFi.localIP().toString();

  JsonObject report = doc["reporthost"].to<JsonObject>();
  CibleReporting cible = cibleReporting();
  report["target_ip"]   = cible.ip[0] ? cible.ip : "0.0.0.0";
  report["target_port"] = cible.port;
  report["sp"]          = cible.sp;

  return doc;
}

// ------------------------------------------------------------ serialisation
void envoyerStatus(const esp_model* em) {
  JsonDocument doc = makeJSON_fromstatus(em);
  // serializeJson : compact, UNE ligne -> parfait pour Node-RED (decoupe sur '\n')
  // serializeJsonPretty : lisible pour un humain, mais sur plusieurs lignes !
  serializeJson(doc, Serial);
  Serial.println();
}
