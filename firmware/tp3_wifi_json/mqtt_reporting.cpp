#include "mqtt_reporting.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>

static WiFiClient     clientTcp;  // connexion tcp utilisee par mqtt
static PubSubClient   mqtt(clientTcp);  // client mqtt
static CibleMqtt      cible = {"", MQTT_PORT_DEFAUT, "", 0};  // cible utilisee par la loop
static CibleMqtt      demande;  // cible recue par le formulaire
static volatile bool  demandeEnAttente = false;  // une nouvelle cible attend d etre appliquee
static unsigned long  tDernierEnvoi = 0;  // date de la derniere publication (ms)
static unsigned long  tDerniereTentative = 0;  // date de la derniere tentative de connexion (ms)
static char           idClient[32];  // identifiant unique aupres du broker

void demarrerMqtt() {
  snprintf(idClient, sizeof(idClient), "ESP32-%s", WiFi.macAddress().c_str());
  mqtt.setBufferSize(MQTT_TAILLE_BUFFER);
}

CibleMqtt cibleMqtt() {
  return demandeEnAttente ? demande : cible;
}

const char* etatMqtt() {
  if (cible.sp == 0) return "arrete";
  return mqtt.connected() ? "connecte" : "deconnecte";
}

static bool hoteValide(const String& h) {
  if (h.length() == 0 || h.length() > 63) return false;
  for (char c : h) if (!isAlphaNumeric(c) && c != '.' && c != '-') return false;
  return true;
}

static bool topicValide(const String& t) {
  if (t.length() == 0 || t.length() > 64) return false;
  for (char c : t) if (c == '+' || c == '#' || c < 33 || c > 126) return false;
  return true;
}

static bool lireEntier(const String& s, long mini, long maxi, long& resultat) {
  if (s.length() == 0 || s.length() > 5) return false;
  for (char c : s) if (!isDigit(c)) return false;
  resultat = s.toInt();
  return resultat >= mini && resultat <= maxi;
}

bool demanderCibleMqtt(String broker, String port, String topic, String sp, String& erreur) {
  broker.trim(); port.trim(); topic.trim(); sp.trim();
  CibleMqtt nouvelle = cibleMqtt();  // cible en cours de validation
  long valeur;  // valeur numerique lue

  if (broker.length()) {
    if (!hoteValide(broker)) { erreur = "broker invalide"; return false; }
    strlcpy(nouvelle.broker, broker.c_str(), sizeof(nouvelle.broker));
  }
  if (port.length()) {
    if (!lireEntier(port, 1, 65535, valeur)) { erreur = "port invalide (1 a 65535)"; return false; }
    nouvelle.port = valeur;
  }
  if (topic.length()) {
    if (!topicValide(topic)) { erreur = "topic invalide (sans espace, + ni #)"; return false; }
    strlcpy(nouvelle.topic, topic.c_str(), sizeof(nouvelle.topic));
  }
  if (sp.length()) {
    if (!lireEntier(sp, 0, 3600, valeur)) { erreur = "periode invalide (0 a 3600 s)"; return false; }
    nouvelle.sp = valeur;
  }
  if (nouvelle.sp > 0 && (nouvelle.broker[0] == '\0' || nouvelle.topic[0] == '\0')) {
    erreur = "il faut un broker et un topic pour publier";
    return false;
  }
  if (demandeEnAttente) { erreur = "demande precedente en cours, reessayez"; return false; }

  demande = nouvelle;
  demandeEnAttente = true;
  return true;
}

static bool connecter() {
  if (mqtt.connected()) return true;
  if (tDerniereTentative != 0 && millis() - tDerniereTentative < MQTT_RECONNEXION_MS) return false;
  tDerniereTentative = millis();
  Serial.printf("MQTT : connexion a %s:%u ... ", cible.broker, cible.port);
  if (mqtt.connect(idClient)) {
    Serial.println("ok");
    return true;
  }
  Serial.printf("echec, rc=%d\n", mqtt.state());
  return false;
}

void gererMqtt(const esp_model* em) {
  if (demandeEnAttente) {
    cible = demande;
    demandeEnAttente = false;
    if (mqtt.connected()) mqtt.disconnect();
    mqtt.setServer(cible.broker, cible.port);
    tDerniereTentative = 0;
    tDernierEnvoi = millis() - cible.sp * 1000UL;
    if (cible.sp == 0) Serial.println("Reporting MQTT : arrete");
    else Serial.printf("Reporting MQTT : %s:%u topic %s toutes les %u s\n", cible.broker, cible.port, cible.topic, cible.sp);
  }

  if (cible.sp == 0 || WiFi.status() != WL_CONNECTED) return;
  if (!connecter()) return;
  mqtt.loop();

  if (millis() - tDernierEnvoi < cible.sp * 1000UL) return;
  tDernierEnvoi = millis();

  String corps;  // json publie
  serializeJson(makeJSON_fromstatus(em), corps);
  bool ok = mqtt.publish(cible.topic, corps.c_str());  // true si le message est parti
  Serial.printf("MQTT publish %s (%u octets) -> %s\n", cible.topic, corps.length(), ok ? "ok" : "ECHEC");
}