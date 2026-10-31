
#include "config.h"
#include "capteurs.h"
#include "actionneurs.h"
#include "regulation.h"
#include "incendie.h"
#include "commandes.h"
#include "makejson.h"
#include "wifi_utils.h"
#include "serveur_web.h"
#include "reporting.h"
#include "mqtt_reporting.h"
#include "stockage.h"

Parametres params;

static EtatTemp      etat = EtatTemp::CONFORT;
static unsigned long tMesure = 0, tCligno = 0, tRapport = 0;
static bool          cligno = false;
esp_model     modele;             
static bool climOn = false, radOn = false;

static void appliquerSorties(float t) {
  bool feu = (niveauFeu() == NiveauFeu::FEU);

  climOn = !feu && (params.forceClim      < 0 ? etat == EtatTemp::CHAUD : params.forceClim      == 1);
  radOn  = !feu && (params.forceRadiateur < 0 ? etat == EtatTemp::FROID : params.forceRadiateur == 1);
  climatisation(climOn);
  radiateur(radOn);
  ventilateur(feu ? 0 : calculerDutyVentilo(t, etat));

  if (feu) return;
  ledAlarme(false);
  if      (etat == EtatTemp::CHAUD) bandeCouleur(255, 0, 0);
  else if (etat == EtatTemp::FROID) bandeCouleur(0, 0, 255);
  else                              bandeCouleur(0, 255, 0);
}

static void clignoterAlarme() {
  cligno = !cligno;
  ledAlarme(cligno);
  if (cligno) bandeCouleur(255, 0, 0); else bandeCouleur(0, 0, 0);
}

static void mettreAJourModele(float t, int lum) {
  bool feu = (niveauFeu() == NiveauFeu::FEU);

  modele.temperature       = t;
  modele.luminosity        = lum;
  modele.regulationRunning = !isnan(t) && !feu;   // HALT : pas de mesure ou incendie
  modele.fireDetected      = feu;
  modele.heaterState       = radOn;
  modele.coolerState       = climOn;
  modele.fanSpeed          = (int)(dutyVentilateur() * 100 / 255);
  modele.lowThreshold      = params.seuilBas;
  modele.highThreshold     = params.seuilHaut;
  modele.uptimeS           = millis() / 1000;
}

// ------------------------------------------------------------ setup / loop
void setup() {
  Serial.begin(VITESSE_SERIE);
  delay(500);
  Serial.println("\n=== TP2 IoT : status JSON pour Node-RED ===");

  actionneursInit();
  capteursInit();
    if (monterStockage()) {
    listerFichiers();          // TODO du prof
    chargerConfig();           // Wi-Fi + seuils depuis la flash
  }
  wificonnect_basic(WIFI_HOSTNAME, cfgWifi.ssid, cfgWifi.password);
  if (WiFi.status() == WL_CONNECTED) wifi_printstatus(0);
  remplirIdentite(&modele);
  demarrerMqtt();
  afficherReglages();
  tMesure = tRapport = millis();
  demarrerServeurWeb();
}

void loop() {
  lireCommandes();
  gererServeurWeb();
  gererReporting(&modele);
  gererMqtt(&modele);
  unsigned long maintenant = millis();

  if (niveauFeu() == NiveauFeu::FEU && maintenant - tCligno >= PERIODE_CLIGNO_MS) {
    tCligno = maintenant;
    clignoterAlarme();
  }

  if (maintenant - tMesure < PERIODE_MESURE_MS) return;
  tMesure = maintenant;

  // 1. MESURER
  float t   = lireTemperature();
  int   lum = lireLumiere();

  // 2. DECIDER
  if (!isnan(t)) etat = calculerEtat(t, etat);
  if (analyserIncendie(lum, t))
    Serial.printf("{\"alerte\":\"%s\",\"lum\":%d}\n", nomNiveau(niveauFeu()), lum);

  // 3. AGIR
  appliquerSorties(isnan(t) ? params.seuilHaut : t);

  // 4. RAPPORTER : le status JSON toutes les PERIODE_RAPPORT_S secondes
  mettreAJourModele(t, lum);
  if (maintenant - tRapport >= PERIODE_RAPPORT_S * 1000UL) {
    tRapport = maintenant;
    envoyerStatus(&modele);
  }
}
