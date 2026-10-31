#include "regulation.h"
#include "config.h"

// On n'entre pas et on ne sort pas d'un etat au meme seuil (hysteresis).
EtatTemp calculerEtat(float t, EtatTemp actuel) {
  const float SB = params.seuilBas;
  const float SH = params.seuilHaut;

  switch (actuel) {
    case EtatTemp::CONFORT:
      if (t > SH) return EtatTemp::CHAUD;
      if (t < SB) return EtatTemp::FROID;
      return EtatTemp::CONFORT;

    case EtatTemp::CHAUD:
      if (t < SH - HYSTERESIS) return (t < SB) ? EtatTemp::FROID : EtatTemp::CONFORT;
      return EtatTemp::CHAUD;

    case EtatTemp::FROID:
      if (t > SB + HYSTERESIS) return (t > SH) ? EtatTemp::CHAUD : EtatTemp::CONFORT;
      return EtatTemp::FROID;
  }
  return EtatTemp::CONFORT;
}

// Vitesse proportionnelle au depassement : DUTY_MIN a SH, 255 a SH + PLAGE_VENTILO.
uint32_t calculerDutyVentilo(float t, EtatTemp etat) {
  if (etat != EtatTemp::CHAUD) return 0;
  float x = (t - params.seuilHaut) / PLAGE_VENTILO;
  x = constrain(x, 0.0f, 1.0f);
  return FAN_DUTY_MIN + (uint32_t)(x * (255 - FAN_DUTY_MIN));
}

const char* nomEtat(EtatTemp e) {
  switch (e) {
    case EtatTemp::FROID:  return "FROID";
    case EtatTemp::CHAUD:  return "CHAUD";
    default:               return "CONFORT";
  }
}
