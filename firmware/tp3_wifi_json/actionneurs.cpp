#include "actionneurs.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel strip(NUMLEDS, PIN_STRIP, NEO_GRB + NEO_KHZ800);
static uint32_t dutyActuel = 0;

// ------------------------------------------------------------ PWM (core 2.x et 3.x)
static void pwmInit() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_FAN, FAN_FREQ, FAN_RES);   // nouvelle API : on travaille avec la broche
#else
  ledcSetup(0, FAN_FREQ, FAN_RES);          // ancienne API : canal 0
  ledcAttachPin(PIN_FAN, 0);
#endif
}

static void pwmEcrire(uint32_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_FAN, duty);
#else
  ledcWrite(0, duty);
#endif
}

// ------------------------------------------------------------ init
void actionneursInit() {
  pinMode(PIN_CLIM, OUTPUT);
  pinMode(PIN_RADIATEUR, OUTPUT);
  pinMode(PIN_ALARME, OUTPUT);

  pwmInit();
  pwmEcrire(0);

  strip.begin();
  strip.setBrightness(LUMINOSITE_STRIP);
  strip.clear();
  strip.show();
}

// ------------------------------------------------------------ sorties
void climatisation(bool on) { digitalWrite(PIN_CLIM, on ? HIGH : LOW); }
void radiateur(bool on)     { digitalWrite(PIN_RADIATEUR, on ? HIGH : LOW); }
void ledAlarme(bool on)     { digitalWrite(PIN_ALARME, on ? HIGH : LOW); }

void ventilateur(uint32_t duty) {
  if (duty == dutyActuel) return;
  if (dutyActuel == 0 && duty > 0) {        // ventilo 2 fils arrete : coup de pouce
    pwmEcrire(255);
    delay(FAN_KICK_MS);
  }
  pwmEcrire(duty);
  dutyActuel = duty;
}

uint32_t dutyVentilateur() { return dutyActuel; }

void bandeCouleur(uint8_t r, uint8_t g, uint8_t b) {
  strip.fill(strip.Color(r, g, b));
  strip.show();
}
