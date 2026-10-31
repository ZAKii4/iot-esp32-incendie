/*
 * actionneurs.h - Commande des sorties
 *   - LEDs climatisation / radiateur
 *   - ventilateur 2 fils en PWM (via transistor), avec demarrage assiste
 *   - bande de LEDs NeoPixel
 *   - LED d'alarme de la carte
 */
#pragma once
#include <Arduino.h>


void actionneursInit();

void climatisation(bool on);
void radiateur(bool on);

void     ventilateur(uint32_t duty);   // 0..255
uint32_t dutyVentilateur();

void bandeCouleur(uint8_t r, uint8_t g, uint8_t b);
void ledAlarme(bool on);
