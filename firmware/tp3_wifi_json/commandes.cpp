#include "commandes.h"
#include "config.h"
#include "capteurs.h"
#include "stockage.h"

void afficherReglages() {
  Serial.printf("SB=%.1f  SH=%.1f  SF=%.1f  D=%d (seuil lum=%d)  T=%s\n",
                params.seuilBas, params.seuilHaut, params.seuilTempFeu,
                params.deltaLumFeu, referenceLumiere() + params.deltaLumFeu,
                isnan(params.tSimulee) ? "capteur" : String(params.tSimulee, 1).c_str());
  if (params.seuilBas >= params.seuilHaut)
    Serial.println("ATTENTION : SB doit etre strictement inferieur a SH !");
}

void lireCommandes() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();

  if (cmd.startsWith("SB ")) { params.seuilBas  = cmd.substring(3).toFloat(); sauverSeuils(); }
  else if (cmd.startsWith("SH ")) { params.seuilHaut = cmd.substring(3).toFloat(); sauverSeuils(); }
  else if (cmd.startsWith("SF "))  params.seuilTempFeu = cmd.substring(3).toFloat();
  else if (cmd.startsWith("D "))   params.deltaLumFeu  = cmd.substring(2).toInt();
  else if (cmd == "T OFF")         params.tSimulee     = NAN;
  else if (cmd.startsWith("T "))   params.tSimulee     = cmd.substring(2).toFloat();
  else if (cmd == "CAL")           calibrerLumiere();
  else if (cmd != "?") {
    Serial.println("Commandes : SB <C> | SH <C> | SF <C> | D <ecart> | T <C> | T OFF | CAL | ?");
    return;
  }
  afficherReglages();
}
