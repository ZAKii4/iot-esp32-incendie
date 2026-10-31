/*
 * incendie.h - Detection d'incendie par fusion de capteurs (edge computing)
 *
 *   FEU = (lumiere > reference + delta) ET (temperature > seuil feu)
 *         pendant CONFIRMATIONS_FEU mesures consecutives.
 *   Une seule condition vraie -> PRE_ALERTE.
 *   Temperature indisponible  -> mode degrade : la lumiere seule suffit.
 */
#pragma once
#include <Arduino.h>

enum class NiveauFeu { NORMAL, PRE_ALERTE, FEU };

// Analyse une mesure, met a jour le niveau et renvoie true si le niveau a change.
bool        analyserIncendie(int lumiere, float temperature);
NiveauFeu   niveauFeu();
int         confirmationsFeu();
const char* nomNiveau(NiveauFeu n);
