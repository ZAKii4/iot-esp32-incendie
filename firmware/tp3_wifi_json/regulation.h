/*
 * regulation.h - Regulation de temperature
 *   Machine a 3 etats avec hysteresis :
 *     T > SH -> CHAUD   : climatisation + ventilateur progressif, bande ROUGE
 *     T < SB -> FROID   : radiateur, bande BLEUE
 *     sinon  -> CONFORT : tout eteint, bande VERTE
 */
#pragma once
#include <Arduino.h>

enum class EtatTemp { FROID, CONFORT, CHAUD };

EtatTemp    calculerEtat(float t, EtatTemp actuel);
uint32_t    calculerDutyVentilo(float t, EtatTemp etat);
const char* nomEtat(EtatTemp e);
