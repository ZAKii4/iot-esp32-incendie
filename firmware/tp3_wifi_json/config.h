/*
 * config.h - Brochage et parametres du projet
 * ------------------------------------------------------------
 * C'est le SEUL fichier a modifier pour adapter le montage.
 */
#pragma once
#include <Arduino.h>

// ===================== BROCHAGE (impose par le sujet) =====================
constexpr int PIN_TEMP      = 23;  // DS18B20 (OneWire)
constexpr int PIN_CLIM      = 19;  // LED verte  = climatisation
constexpr int PIN_RADIATEUR = 21;  // LED rouge  = radiateur
constexpr int PIN_FAN       = 27;  // transistor qui alimente le ventilateur 2 fils
constexpr int PIN_STRIP     = 13;  // DIN de la bande de LEDs
constexpr int PIN_LUM       = 33;  // capteur de lumiere (ADC1_CH5)
constexpr int PIN_ALARME    = 2;   // LED bleue de la carte = alarme incendie

// ===================== MATERIEL =====================
constexpr int      NUMLEDS          = 2;      // nombre de LEDs de la bande
constexpr uint8_t  LUMINOSITE_STRIP = 100;    // 0..255
constexpr bool     LUM_INVERSEE     = true ;   // true si la valeur BAISSE quand on eclaire

constexpr uint32_t FAN_FREQ     = 25000;      // Hz : au-dessus de l'audible
constexpr uint8_t  FAN_RES      = 8;          // bits -> duty 0..255
constexpr uint8_t  FAN_DUTY_MIN = 100;        // ~40 % : vitesse mini d'un ventilo 2 fils
constexpr int      FAN_KICK_MS  = 300;        // impulsion a 100 % pour demarrer

// ===================== REGULATION =====================
constexpr float HYSTERESIS    = 0.3f;         // C : evite le clignotement autour d'un seuil
constexpr float PLAGE_VENTILO = 4.0f;         // C : a SH + 4 C le ventilateur est a 100 %

// ===================== INCENDIE =====================
constexpr int CONFIRMATIONS_FEU = 3;          // mesures "feu" consecutives avant alarme

// ===================== CADENCE =====================
constexpr unsigned long PERIODE_MESURE_MS = 1000; // >= 750 ms (conversion DS18B20 12 bits)
constexpr unsigned long PERIODE_CLIGNO_MS = 250;
constexpr long          VITESSE_SERIE     = 9600;

// ===================== RAPPORT JSON (TP2) =====================
// Toutes les PERIODE_RAPPORT_S secondes, l'ESP envoie son "status" JSON
// sur UNE ligne du port serie (Node-RED decoupe les messages sur '\n').
constexpr int PERIODE_RAPPORT_S = 2;          // = champ "reporthost.sp"

// Constantes d'identite et de localisation (champs "info" et "location")
constexpr const char* INFO_USER    = "Malak Zaoufy & Zakaria Khamlich";
constexpr const char* INFO_LOC     = "EMSI Casablanca";
constexpr const char* LOC_ROOM     = "Salle TP IoT";
constexpr const char* LOC_ADDRESS  = "EMSI Casablanca - site CFC";
constexpr double      LOC_LAT      = 33.5731;   // Casablanca (centre-ville)
constexpr double      LOC_LON      = -7.5898;   // a remplacer par la vraie position

// Le Wi-Fi n'est pas encore utilise : on met des valeurs "NOP" (consigne du prof)
// Wi-Fi (TP3) : le reseau auquel l'ESP se connecte au demarrage
constexpr const char* WIFI_SSID     = "MON_RESEAU";
constexpr const char* WIFI_PASSWORD = "MON_MOT_DE_PASSE";
constexpr const char* WIFI_HOSTNAME = "ESP32-Zaoufy-Khamlich";
//constexpr const char* WIFI_SSID     = "MON_RESEAU";
//constexpr const char* WIFI_PASSWORD = "MON_MOT_DE_PASSE";

constexpr const char* REPORT_IP    = "127.0.0.1"; // machine qui ecoute (Node-RED)
constexpr int         REPORT_PORT  = 1880;  
constexpr int         HTTP_PORT         = 80;
constexpr const char* REPORT_PATH       = "/esp";
constexpr uint16_t    REPORT_TIMEOUT_MS = 1500;
constexpr uint16_t      MQTT_PORT_DEFAUT    = 1883;  // port mqtt standard
constexpr uint16_t      MQTT_TAILLE_BUFFER  = 1024;  // taille max d un message mqtt (octets)
constexpr unsigned long MQTT_RECONNEXION_MS = 5000;  // attente entre deux tentatives de connexion (ms)
// ===================== PARAMETRES MODIFIABLES EN MARCHE =====================
// (via le moniteur serie, voir commandes.cpp)
struct Parametres {
  float seuilBas      = 22.0f;  // SB : en dessous -> radiateur
  float seuilHaut     = 26.0f;  // SH : au dessus  -> climatisation + ventilateur
  float seuilTempFeu  = 30.0f;  // au dessus -> chaleur "suspecte"
  int   deltaLumFeu   = 400;    // hausse de lumiere "suspecte" (0..4095)
  float tSimulee      = NAN; 
  int8_t forceClim      = -1;
  int8_t forceRadiateur = -1;
};

extern Parametres params;       // defini dans tp1_iot.ino
