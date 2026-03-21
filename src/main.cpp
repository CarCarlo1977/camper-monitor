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
const unsigned long AH_SAVE_INTERVAL  =  10000UL;  // 10 sec (ahUsed)

unsigned long lastSensorMs = 0, lastSerialMs = 0, lastSaveMs = 0, lastAhSaveMs = 0;

void setupWiFi() {
  if (config.wifiAPMode) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.wifiSSID.c_str(), config.wifiPassword.c_str(), 1, false, 4);
    IPAddress ip(192,168,4,1), gw(192,168,4,1), sn(255,255,255,0);
    WiFi.softAPConfig(ip, gw, sn);
    Serial.printf("[WiFi] AP: %s  IP: %s\n", config.wifiSSID.c_str(),
                  WiFi.softAPIP().toString().c_str());
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSTA_SSID.c_str(), config.wifiSTA_Password.c_str());
    Serial.printf("[WiFi] Connessione a %s", config.wifiSTA_SSID.c_str());
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
      delay(500); Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\n[WiFi] Connesso IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("\n[WiFi] Fallito, AP di fallback");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(config.wifiSSID.c_str(), config.wifiPassword.c_str());
      Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    }
  }
}

void sendSerialJSON() {
  if (!config.debugSerial) return;
  char buf[512];
  snprintf(buf, sizeof(buf),
    "{\"v\":%.2f,\"i\":%.1f,\"w\":%.0f,\"soc\":%.1f,"
    "\"ah\":%.1f,\"vmin\":%.2f,\"vmax\":%.2f,"
    "\"imin\":%.1f,\"imax\":%.1f,"
    "\"tank_gray\":%u,\"tank_black\":%u,"
    "\"adc_blk\":%u,\"adc_g1\":%u,\"adc_g2\":%u,\"adc_g3\":%u,"
    "\"sim\":%u,\"heap\":%u}",
    sensors.voltageBus, sensors.currentA, sensors.powerW,
    sensors.batterySOC, sensors.ahUsed,
    sensors.voltageMin, sensors.voltageMax,
    sensors.currentMin, sensors.currentMax,
    sensors.tankGray, sensors.tankBlack,
    sensors.tankADC[0], sensors.tankADC[1], sensors.tankADC[2], sensors.tankADC[3],
    sensors.simMode ? 1 : 0, ESP.getFreeHeap());
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

  delay(10);
}