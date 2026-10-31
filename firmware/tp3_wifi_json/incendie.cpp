#include "incendie.h"
#include "config.h"
#include "capteurs.h"

static NiveauFeu niveau   = NiveauFeu::NORMAL;
static int       compteur = 0;

bool analyserIncendie(int lumiere, float temperature) {
  // 1. Les deux indices
  bool lumSuspecte     = lumiere > referenceLumiere() + params.deltaLumFeu;
  bool tempDispo       = !isnan(temperature);
  bool chaleurSuspecte = tempDispo && temperature > params.seuilTempFeu;

  // 2. Fusion (ET) ; mode degrade si pas de temperature
  bool indicesFeu = tempDispo ? (lumSuspecte && chaleurSuspecte) : lumSuspecte;

  // 3. Confirmation : N mesures consecutives
  compteur = indicesFeu ? min(compteur + 1, CONFIRMATIONS_FEU) : 0;

  NiveauFeu nouveau;
  if (compteur >= CONFIRMATIONS_FEU)       nouveau = NiveauFeu::FEU;
  else if (lumSuspecte || chaleurSuspecte) nouveau = NiveauFeu::PRE_ALERTE;
  else                                     nouveau = NiveauFeu::NORMAL;

  bool change = (nouveau != niveau);
  niveau = nouveau;
  return change;
}

NiveauFeu niveauFeu()        { return niveau; }
int       confirmationsFeu() { return compteur; }

const char* nomNiveau(NiveauFeu n) {
  switch (n) {
    case NiveauFeu::FEU:        return "FEU";
    case NiveauFeu::PRE_ALERTE: return "PRE_ALERTE";
    default:                    return "NORMAL";
  }
}
