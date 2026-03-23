// ============================================================
//  main.cpp - Camper Monitor ESP32 v3.0
// ============================================================
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "config.h"
#include "sensors.h"
#include "web_server.h"

const unsigned long SENSOR_INTERVAL  =   1000UL;
const unsigned long SERIAL_INTERVAL  =   2000UL;
const unsigned long SAVE_INTERVAL    = 300000UL;  // 5 min (min/max)
const unsigned long AH_SAVE_INTERVAL =  10000UL;  // 10 sec (ahUsed)
const unsigned long WIFI_RETRY_MS    =  30000UL;  // riprova STA ogni 30s

unsigned long lastSensorMs = 0, lastSerialMs = 0, lastSaveMs = 0,
              lastAhSaveMs = 0, lastWifiRetryMs = 0;

void setupWiFi() {
  // AP+STA sempre attivi contemporaneamente
  WiFi.mode(WIFI_AP_STA);

  // --- STA prima: connette e determina il canale ---
  // L'ESP32 ha un solo radio: AP e STA devono usare lo stesso canale
  int staChannel = 1; // default
  if (config.wifiSTA_SSID.length() > 0) {
    Serial.printf("[WiFi] STA SSID: '%s'\n", config.wifiSTA_SSID.c_str());
    Serial.printf("[WiFi] STA PASS: '%s'\n", config.wifiSTA_Password.c_str());
    Serial.printf("[WiFi] STA PASS len: %d\n", config.wifiSTA_Password.length());
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true);
    delay(200);
    WiFi.begin(config.wifiSTA_SSID.c_str(), config.wifiSTA_Password.c_str());
    Serial.printf("[WiFi] STA connessione a %s", config.wifiSTA_SSID.c_str());
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
      delay(500); Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      staChannel = WiFi.channel();
      Serial.printf("\n[WiFi] STA IP: %s  RSSI: %d dBm  CH: %d\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI(), staChannel);
    } else {
      Serial.println("\n[WiFi] STA fallita - solo AP attivo");
      Serial.println("[WiFi] Scansione reti vicine...");
      int n = WiFi.scanNetworks();
      for (int i = 0; i < n; i++) {
        Serial.printf("  [%d] SSID: '%s'  RSSI: %d dBm  CH: %d\n",
                      i+1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
        // Salva il canale della rete target
        if (WiFi.SSID(i) == config.wifiSTA_SSID) staChannel = WiFi.channel(i);
      }
      WiFi.scanDelete();
      WiFi.disconnect(false);
    }
  } else {
    Serial.println("[WiFi] STA SSID non configurato - solo AP attivo");
  }

  // AP avviato sul canale STA (radio condiviso)
  IPAddress apIP(192,168,4,1), apGW(192,168,4,1), apSN(255,255,255,0);
  WiFi.softAPConfig(apIP, apGW, apSN);
  WiFi.softAP(config.wifiSSID.c_str(), config.wifiPassword.c_str(), staChannel, false, 4);
  Serial.printf("[WiFi] AP: %s  IP: %s  CH: %d\n",
                config.wifiSSID.c_str(),
                WiFi.softAPIP().toString().c_str(), staChannel);
}

void sendSerialJSON() {
  if (!config.debugSerial) return;
  bool staOk = (WiFi.status() == WL_CONNECTED);
  char buf[512];
  snprintf(buf, sizeof(buf),
    "{\"v\":%.2f,\"i\":%.2f,\"w\":%.0f,\"soc\":%.1f,"
    "\"ah\":%.1f,\"vmin\":%.2f,\"vmax\":%.2f,"
    "\"imin\":%.2f,\"imax\":%.2f,"
    "\"tank_gray\":%u,\"tank_black\":%u,"
    "\"adc_blk\":%u,\"adc_g1\":%u,\"adc_g2\":%u,\"adc_g3\":%u,"
    "\"sim\":%u,\"heap\":%u,\"sta\":%u}",
    sensors.voltageBus, sensors.currentA, sensors.powerW,
    sensors.batterySOC, sensors.ahUsed,
    sensors.voltageMin, sensors.voltageMax,
    sensors.currentMin, sensors.currentMax,
    sensors.tankGray, sensors.tankBlack,
    sensors.tankADC[0], sensors.tankADC[1], sensors.tankADC[2], sensors.tankADC[3],
    sensors.simMode ? 1 : 0, ESP.getFreeHeap(),
    staOk ? 1 : 0);
  Serial.println(buf);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n======================================");
  Serial.println("  CAMPER MONITOR ESP32 v3.0");
  Serial.println("======================================\n");

  config.load();
  Serial.println("[Config] Caricata");

  if (!LittleFS.begin(true)) Serial.println("[FS] ERRORE mount");
  else Serial.println("[FS] LittleFS OK");

  sensors.batteryCapacityAh = config.batteryCapacityAh;
  sensors.init(config.pinI2C_SDA, config.pinI2C_SCL);

  setupWiFi();
  delay(300);
  webServer.init();

  Serial.println("\n======================================");
  Serial.println("  SISTEMA PRONTO");
  Serial.println("======================================\n");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSensorMs >= SENSOR_INTERVAL) {
    lastSensorMs = now;
    sensors.readBattery();
    sensors.updateCoulombCounter();
    sensors.updateTanksFSM();
  }

  if (now - lastSerialMs >= SERIAL_INTERVAL) {
    lastSerialMs = now;
    sendSerialJSON();
  }

  // Salvataggio periodico min/max (ogni 5 min)
  if (now - lastSaveMs >= SAVE_INTERVAL) {
    lastSaveMs = now;
    config.saveMinMax(sensors.voltageMin, sensors.voltageMax,
                      sensors.currentMin, sensors.currentMax);
  }

  // Salvataggio ahUsed ogni 10 secondi per SOC preciso dopo reboot
  if (now - lastAhSaveMs >= AH_SAVE_INTERVAL) {
    lastAhSaveMs = now;
    config.saveAhUsed(sensors.ahUsed);
  }

  // Riconnessione STA automatica se cade la rete
  if (config.wifiSTA_SSID.length() > 0 &&
      WiFi.status() != WL_CONNECTED &&
      now - lastWifiRetryMs >= WIFI_RETRY_MS) {
    lastWifiRetryMs = now;
    Serial.println("[WiFi] STA disconnessa, riconnessione...");
    WiFi.disconnect(false);
    delay(100);
    WiFi.begin(config.wifiSTA_SSID.c_str(), config.wifiSTA_Password.c_str());
  }

  delay(10);
}