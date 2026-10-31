# Objet connecté ESP32 : régulation de température et détection d'incendie

Projet IoT réalisé à l'EMSI par **Malak Zaoufy** et **Zakaria Khamlich**.

Un ESP32 régule la température d'une pièce, détecte un départ d'incendie et rend compte de son état en **JSON** via **HTTP** et **MQTT**, supervisé par un dashboard **Node-RED**.

![Dashboard](docs/captures/dashboard.png)

## Fonctionnalités

**Objet (ESP32, C++ / Arduino)**
- Régulation FROID / CONFORT / CHAUD avec hystérésis (LED radiateur, LED clim, bande de LEDs)
- Ventilateur 2 fils piloté en PWM, proportionnel à l'écart de température
- Détection d'incendie par fusion lumière + température, avec moyenne glissante sur 5 s (insensible aux flashs)
- Configuration WiFi et seuils stockés en flash (LittleFS), persistants après redémarrage

**Communication**
- État complet de l'objet au format JSON (ArduinoJson)
- Serveur web asynchrone : page d'administration et API REST
- Reporting périodique en HTTP POST et en MQTT (TLS sur le port 8883 si le 1883 est filtré)

**Supervision**
- Dashboard Node-RED : jauges, courbes, alertes, carte (worldmap)
- Validateur Python du JSON (JSON Schema, durci et testé par fuzzing)

## Architecture

## Matériel

| Élément | GPIO |
|---|---|
| Capteur DS18B20 (OneWire) | 23 |
| LED climatisation / radiateur | 19 / 21 |
| Ventilateur (via transistor, PWM 25 kHz) | 27 |
| Bande de LEDs NeoPixel | 13 |
| Capteur de lumière (ADC1) | 33 |
| LED d'alarme | 2 |

## API HTTP de l'ESP

| Requête | Rôle |
|---|---|
| `GET /` | page d'administration (placeholders remplis par l'ESP) |
| `GET /value?temperature&light&fire&lt&ht` | lecture des valeurs en JSON (404 si nom inconnu) |
| `GET /set?lt=21&ht=27` / `?cool=on\|off\|auto` | modification des seuils ou forçage des actionneurs |
| `POST /target` (ip, port, sp) | cible du reporting HTTP |
| `POST /targetmqtt` (broker, port, topic, sp) | cible du reporting MQTT |

## Installation

1. Arduino IDE, carte **ESP32 Dev Module**, partition **Default 4MB with spiffs**.
2. Bibliothèques : ArduinoJson, OneWire, DallasTemperature, Adafruit NeoPixel, ESP Async WebServer, Async TCP, PubSubClient.
3. Copier `firmware/tp3_wifi_json/data/config.example.json` en `config.json` et y mettre son WiFi.
4. Téléverser le firmware, puis le dossier `data/` (Upload LittleFS).
5. Node-RED : importer `node-red/flows.json` (modules node-red-dashboard, node-red-contrib-web-worldmap).

## Ce que nous avons appris
- Filtrer un capteur réel (médiane, moyenne glissante, hystérésis) est aussi important que le lire.
- Un handler de serveur asynchrone ne doit jamais lire un capteur ni appeler `delay()`.
- Le découplage MQTT permet de changer de broker ou de port sans toucher au reste du système.
