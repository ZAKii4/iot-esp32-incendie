#pragma once
#include <WiFi.h>
#include <WiFiMulti.h>

#define WiFiMaxTry 10            // nombre maximum de tentatives
#define SaveDisconnectTime 1000  // pause en ms entre deux tentatives

void wifi_printstatus(int C);
void wificonnect_basic(String hostname, String ssid, String passwd);