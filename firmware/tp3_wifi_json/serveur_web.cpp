#include "serveur_web.h"
#include "config.h"
#include "makejson.h"
#include "reporting.h"
#include "mqtt_reporting.h"
#include "stockage.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

extern esp_model modele;

static AsyncWebServer serveur(HTTP_PORT);
static volatile bool  sauvegardeDemandee = false;

static const char* nomMode(int8_t mode) {
  if (mode < 0) return "auto";
  return mode ? "on" : "off";
}

static String processor(const String& var) {
  if (var == "IDENT")       return identifiantESP();
  if (var == "USER")        return INFO_USER;
  if (var == "UPTIME")      return String(modele.uptimeS);
  if (var == "WHERE")       return String(LOC_ROOM) + " - " + LOC_ADDRESS;
  if (var == "GPS")         return String(LOC_LAT, 4) + ", " + String(LOC_LON, 4);
  if (var == "SSID")        return WiFi.isConnected() ? WiFi.SSID() : String("NOP");
  if (var == "MAC")         return modele.mac;
  if (var == "IP")          return WiFi.localIP().toString();
  if (var == "RSSI")        return String(WiFi.RSSI());
  if (var == "HOSTNAME")    return WIFI_HOSTNAME;
  if (var == "TEMPERATURE") return isnan(modele.temperature) ? String("--") : String(modele.temperature, 2);
  if (var == "LIGHT")       return String(modele.luminosity);
  if (var == "COOLER")      return String(modele.coolerState ? "ON" : "OFF") + " (mode " + nomMode(params.forceClim) + ")";
  if (var == "HEATER")      return String(modele.heaterState ? "ON" : "OFF") + " (mode " + nomMode(params.forceRadiateur) + ")";
  if (var == "FAN")         return String(modele.fanSpeed);
  if (var == "FIRE")        return modele.fireDetected ? "OUI" : "non";
  if (var == "REGUL")       return modele.regulationRunning ? "RUNNING" : "HALT";
  if (var == "LT")          return String(params.seuilBas, 1);
  if (var == "HT")          return String(params.seuilHaut, 1);
  CibleReporting c = cibleReporting();
  if (var == "PRT_IP")      return c.ip[0] ? String(c.ip) : String("aucune");
  if (var == "PRT_PORT")    return String(c.port);
  if (var == "PRT_T")       return String(c.sp);
  CibleMqtt m = cibleMqtt();  // cible mqtt
  if (var == "MQTT_BROKER") return m.broker[0] ? String(m.broker) : String("aucun");
  if (var == "MQTT_PORT")   return String(m.port);
  if (var == "MQTT_TOPIC")  return m.topic[0] ? String(m.topic) : String("aucun");
  if (var == "MQTT_T")      return String(m.sp);
  if (var == "MQTT_ETAT")   return etatMqtt();
  return "[" + var + " ?]";
}

static void envoyerJson(AsyncWebServerRequest* req, int code, const JsonDocument& doc) {
  String texte;
  serializeJson(doc, texte);
  req->send(code, "application/json", texte);
}

static void envoyerErreur(AsyncWebServerRequest* req, int code, const char* message, const String& param) {
  JsonDocument doc;
  doc["error"] = message;
  if (param.length()) doc["param"] = param;
  envoyerJson(req, code, doc);
}

static float arrondi2(float x) { return roundf(x * 100.0f) / 100.0f; }

static void routeValue(AsyncWebServerRequest* req) {
  size_t n = req->params();
  if (n == 0) { envoyerErreur(req, 400, "aucune valeur demandee", ""); return; }
  JsonDocument doc;
  for (size_t i = 0; i < n; i++) {
    const String& nom = req->getParam(i)->name();
    if (nom == "temperature") {
      if (isnan(modele.temperature)) doc[nom] = nullptr;
      else                           doc[nom] = arrondi2(modele.temperature);
    }
    else if (nom == "light") doc[nom] = modele.luminosity;
    else if (nom == "fire")  doc[nom] = modele.fireDetected;
    else if (nom == "lt")    doc[nom] = arrondi2(params.seuilBas);
    else if (nom == "ht")    doc[nom] = arrondi2(params.seuilHaut);
    else { envoyerErreur(req, 404, "valeur inconnue", nom); return; }
  }
  envoyerJson(req, 200, doc);
}

static bool lireReel(const String& texte, float& resultat) {
  if (texte.length() == 0 || texte.length() > 8) return false;
  char* fin;
  double v = strtod(texte.c_str(), &fin);
  if (*fin != '\0' || !isfinite(v)) return false;
  resultat = v;
  return true;
}

static bool lireMode(const String& texte, int8_t& mode) {
  if      (texte.equalsIgnoreCase("on"))   mode = 1;
  else if (texte.equalsIgnoreCase("off"))  mode = 0;
  else if (texte.equalsIgnoreCase("auto")) mode = -1;
  else return false;
  return true;
}

static void routeSet(AsyncWebServerRequest* req) {
  size_t n = req->params();
  if (n == 0) { envoyerErreur(req, 400, "aucun parametre", ""); return; }

  float  lt = params.seuilBas, ht = params.seuilHaut;
  int8_t clim = params.forceClim, rad = params.forceRadiateur;
  bool   seuilsModifies = false;

  for (size_t i = 0; i < n; i++) {
    const String& nom    = req->getParam(i)->name();
    const String& valeur = req->getParam(i)->value();
    bool ok;
    if      (nom == "lt")   { ok = lireReel(valeur, lt); seuilsModifies = true; }
    else if (nom == "ht")   { ok = lireReel(valeur, ht); seuilsModifies = true; }
    else if (nom == "cool") ok = lireMode(valeur, clim);
    else if (nom == "heat") ok = lireMode(valeur, rad);
    else { envoyerErreur(req, 404, "parametre inconnu", nom); return; }
    if (!ok) { envoyerErreur(req, 400, "valeur invalide", nom); return; }
  }
  if (lt < -55 || ht > 125 || lt >= ht) {
    envoyerErreur(req, 400, "seuils incoherents : il faut -55 <= lt < ht <= 125", ""); return;
  }
  if (clim == 1 && rad == 1) {
    envoyerErreur(req, 409, "cool et heat ne peuvent pas etre forces a on ensemble", ""); return;
  }

  params.seuilBas = lt;  params.seuilHaut = ht;
  params.forceClim = clim;  params.forceRadiateur = rad;
  if (seuilsModifies) sauvegardeDemandee = true;

  JsonDocument doc;
  doc["lt"] = arrondi2(lt);  doc["ht"] = arrondi2(ht);
  doc["cool"] = nomMode(clim);  doc["heat"] = nomMode(rad);
  envoyerJson(req, 200, doc);
}

static void routeTarget(AsyncWebServerRequest* req) {
  String erreur;
  if (!demanderCible(req->arg("ip"), req->arg("port"), req->arg("sp"), erreur)) {
    req->send(400, "text/plain; charset=utf-8", "Erreur : " + erreur);
    return;
  }
  req->redirect("/", 303);
}
static void routeTargetMqtt(AsyncWebServerRequest* req) {
  String erreur;  // message d erreur de validation
  if (!demanderCibleMqtt(req->arg("broker"), req->arg("port"), req->arg("topic"), req->arg("sp"), erreur)) {
    req->send(400, "text/plain; charset=utf-8", "Erreur : " + erreur);
    return;
  }
  req->redirect("/", 303);
}
void demarrerServeurWeb() {
  serveur.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(LittleFS, "/index.html", "text/html", false, processor);
  });
  serveur.serveStatic("/esp.css", LittleFS, "/esp.css");
  serveur.on("/temperature", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/plain", isnan(modele.temperature) ? String("--") : String(modele.temperature, 2));
  });
  serveur.on("/light", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/plain", String(modele.luminosity));
  });
  serveur.on("/value",  HTTP_GET,  routeValue);
  serveur.on("/set",    HTTP_GET,  routeSet);
  serveur.on("/target", HTTP_POST, routeTarget);
    serveur.on("/targetmqtt", HTTP_POST, routeTargetMqtt);
  serveur.onNotFound([](AsyncWebServerRequest* req) {
    req->send(404, "text/plain", "404 : route inconnue");
  });
  serveur.begin();
  Serial.printf("Serveur HTTP pret : http://%s/\n", WiFi.localIP().toString().c_str());
}

void gererServeurWeb() {
  if (sauvegardeDemandee) {
    sauvegardeDemandee = false;
    sauverSeuils();
  }
}