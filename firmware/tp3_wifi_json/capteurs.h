/*
 * capteurs.h - Lecture des capteurs
 *   - DS18B20 (temperature) en mode NON bloquant, avec simulation possible
 *   - capteur de lumiere (ADC 12 bits), moyenne + correction d'inversion
 */
#pragma once
#include <Arduino.h>

enum class SourceTemp { CAPTEUR, SIMULEE, ABSENTE };

void  capteursInit();

float lireTemperature();          // NAN si aucune temperature disponible
SourceTemp sourceTemperature();   // d'ou vient la derniere temperature

int   lireLumiere();              // 0..4095 (grand = beaucoup de lumiere)
void  calibrerLumiere();          // mesure la lumiere ambiante de reference
int   referenceLumiere();
