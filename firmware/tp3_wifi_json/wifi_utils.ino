#include "wifi_utils.h"
#define USE_SERIAL Serial

void wificonnect_basic(String hostname, String ssid, String passwd) {
  int nbtry = 0;

  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);              // 1. l'ESP devient une STATION (client)
  WiFi.disconnect(true);            //    oublie une éventuelle connexion précédente
  WiFi.setHostname(hostname.c_str());   // nom de l'ESP sur le réseau (avant begin !)

  USE_SERIAL.printf("\nConnexion au SSID : %s", ssid.c_str());
  WiFi.begin(ssid.c_str(), passwd.c_str());   // 2. on donne SSID + mot de passe

  while (WiFi.status() != WL_CONNECTED && nbtry < WiFiMaxTry) {   // 3. on attend
    delay(SaveDisconnectTime);
    USE_SERIAL.print(".");
    nbtry++;
  }
  if (nbtry == WiFiMaxTry)
    USE_SERIAL.println("\nWi-Fi indisponible : on continue HORS LIGNE");     // échec après 10 essais : on redémarre
}

String translateEncryptionType(wifi_auth_mode_t t) {
  switch (t) {
    case WIFI_AUTH_OPEN:            return "Open";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2_ENTERPRISE";
    default:                        return "Autre (WPA3...)";
  }
}

void wifi_printstatus(int C) {
  String s = "WiFi Status :\n";
  s += "\tIP address : "  + WiFi.localIP().toString() + "\n";
  s += "\tMAC address : " + WiFi.macAddress() + "\n";
  s += "\tSSID : "        + WiFi.SSID() + "\n";
  s += "\tRSSI : "        + String(WiFi.RSSI()) + " dBm\n";
  s += "\tRSSI : "        + String(constrain(2 * (WiFi.RSSI() + 100), 0, 100)) + " %\n";
  s += "\tBSSID : "       + WiFi.BSSIDstr() + "\n";
  s += "\tEncryption : "  + translateEncryptionType(WiFi.encryptionType(0)) + "\n";
  USE_SERIAL.print(s);
}