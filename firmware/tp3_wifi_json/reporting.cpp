#include "reporting.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>

static CibleReporting cible = {"", REPORT_PORT, 0};
static CibleReporting demande;
static volatile bool  demandeEnAttente = false;
static unsigned long  tDernierEnvoi = 0;

CibleReporting cibleReporting() {
  return demandeEnAttente ? demande : cible;
}

static bool lireEntier(const String& s, long mini, long maxi, long& resultat) {
  if (s.length() == 0 || s.length() > 5) return false;
  for (char c : s) if (!isDigit(c)) return false;
  resultat = s.toInt();
  return resultat >= mini && resultat <= maxi;
}

bool demanderCible(String ip, String port, String sp, String& erreur) {
  ip.trim(); port.trim(); sp.trim();
  CibleReporting nouvelle = cibleReporting();
  long valeur;

  if (ip.length()) {
    IPAddress adresse;
    if (ip.length() > 15 || !adresse.fromString(ip) || adresse.type() != IPv4) {
      erreur = "adresse IP invalide";
      return false;
    }
    strlcpy(nouvelle.ip, ip.c_str(), sizeof(nouvelle.ip));
  }
  if (port.length()) {
    if (!lireEntier(port, 1, 65535, valeur)) { erreur = "port invalide (1 a 65535)"; return false; }
    nouvelle.port = valeur;
  }
  if (sp.length()) {
    if (!lireEntier(sp, 0, 3600, valeur)) { erreur = "periode invalide (0 a 3600 s)"; return false; }
    nouvelle.sp = valeur;
  }
  if (nouvelle.sp > 0 && nouvelle.ip[0] == '\0') {
    erreur = "il faut une adresse IP pour activer le reporting";
    return false;
  }
  if (demandeEnAttente) { erreur = "demande precedente en cours, reessayez"; return false; }

  demande = nouvelle;
  demandeEnAttente = true;
  return true;
}

void gererReporting(const esp_model* em) {
  if (demandeEnAttente) {
    cible = demande;
    demandeEnAttente = false;
    tDernierEnvoi = millis() - cible.sp * 1000UL;
    if (cible.sp == 0) Serial.println("Reporting HTTP : arrete");
    else Serial.printf("Reporting HTTP : %s:%u toutes les %u s\n", cible.ip, cible.port, cible.sp);
  }

  if (cible.sp == 0 || WiFi.status() != WL_CONNECTED) return;
  if (millis() - tDernierEnvoi < cible.sp * 1000UL) return;
  tDernierEnvoi = millis();

  char url[80];
  snprintf(url, sizeof(url), "http://%s:%u%s?mac=%s", cible.ip, cible.port, REPORT_PATH, em->mac);

  String corps;
  serializeJson(makeJSON_fromstatus(em), corps);

  HTTPClient http;
  http.setConnectTimeout(REPORT_TIMEOUT_MS);
  http.setTimeout(REPORT_TIMEOUT_MS);
  if (!http.begin(url)) { Serial.printf("POST %s -> URL invalide\n", url); return; }
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(corps);
  if (code > 0) Serial.printf("POST %s -> %d\n", url, code);
  else          Serial.printf("POST %s -> echec : %s\n", url, http.errorToString(code).c_str());
  http.end();
}