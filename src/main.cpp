// ============================================================
//  main.cpp - Camper Monitor ESP32 v3.0
// ============================================================
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "config.h"
#include "sensors.h"
#include "web_server.h"
#include <esp_ota_ops.h>

const unsigned long SENSOR_INTERVAL  =   1000UL;
const unsigned long SERIAL_INTERVAL  =   2000UL;
const unsigned long SAVE_INTERVAL    = 300000UL;  // 5 min (min/max)
const unsigned long AH_SAVE_INTERVAL =  60000UL;  // 1 min (ahUsed)
const unsigned long WIFI_RETRY_MS    =  30000UL;  // riprova STA ogni 30s

unsigned long lastSensorMs = 0, lastSerialMs = 0, lastSaveMs = 0,
              lastAhSaveMs = 0, lastWifiRetryMs = 0;

#include "esp_bt.h" // <--- AGGIUNGI IN ALTO SE MANCA

void setupWiFi() {
  Serial.println("\n[WiFi] Hard Reset Radio e Coesistenza...");

  // 1. SPEGNIMENTO TOTALE (Pulisce i buffer radio e libera l'antenna)
  btStop(); // Forza lo spegnimento del Bluetooth se rimasto attivo
  WiFi.disconnect(true, true); 
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(600);

  // 2. MODALITÀ AP+STA
  WiFi.mode(WIFI_AP_STA);
  
  // 3. SINCRONIZZAZIONE SUL CANALE 6 (Quello della tua rete aziendale)
  int targetChannel = 6; 

  // Configurazione AP (Camper-Monitor)
  IPAddress apIP(192, 168, 4, 1), apGW(192, 168, 4, 1), apSN(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apGW, apSN);
  
  // Avviamo l'AP subito sul canale 6
  WiFi.softAP(config.wifiSSID.c_str(), config.wifiPassword.c_str(), targetChannel, false, 4);
  Serial.printf("[WiFi] AP attivo su CH: %d\n", targetChannel);

  // 4. CONNESSIONE STA
  if (config.wifiSTA_SSID.length() > 0) {
    // DISABILITA IL RISPARMIO ENERGETICO (Fondamentale per reti aziendali)
    // Se la radio va in sleep anche per un millisecondo, il router aziendale chiude la porta
    WiFi.setSleep(false);

    Serial.printf("[WiFi] STA -> %s\n", config.wifiSTA_SSID.c_str());
    
    // Inizia la connessione forzando il canale per non far scansionare la radio
    WiFi.begin(config.wifiSTA_SSID.c_str(), config.wifiSTA_Password.c_str(), targetChannel);

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\n[WiFi] CONNESSO! IP: %s\n", WiFi.localIP().toString().c_str());
      WiFi.setAutoReconnect(true);
    } else {
      // Vediamo cosa dice il driver WiFi sull'errore
      Serial.printf("\n[WiFi] Fallito. Stato: %d\n", (int)WiFi.status());
    }
  }
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

bool scanI2C(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);  // 0 = ACK ricevuto
}

void setup() {
  Serial.begin(115200);
  delay(500);

  esp_ota_mark_app_valid_cancel_rollback();

  Serial.println("\n===========================================");
  Serial.println("  CAMPER MONITOR v2.0 by CarCarlo Caligiuri");
  Serial.println("===========================================\n");

  config.load();
  Serial.println("[Config] Caricata");

  Wire.begin(21, 22);
  delay(100);
  
  Serial.println("[I2C] Scanning...");
  bool found_ina = false;
  
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



  // --- 1. LETTURA SENSORI ---
  if (now - lastSensorMs >= SENSOR_INTERVAL) {
    lastSensorMs = now;
    sensors.readBattery();
    sensors.updateCoulombCounter();
    sensors.updateTanksFSM();
  }

  // --- 2. LOG SERIALE ---
  if (now - lastSerialMs >= SERIAL_INTERVAL) {
    lastSerialMs = now;
    sendSerialJSON();
  }

  // --- 3. SALVATAGGIO MIN/MAX (Periodico) ---
  if (now - lastSaveMs >= SAVE_INTERVAL) {
    lastSaveMs = now;
    config.saveMinMax(sensors.voltageMin, sensors.voltageMax,
                      sensors.currentMin, sensors.currentMax);
  }

 // --- 4. SALVATAGGIO AH (SOC) ---
unsigned long elapsed = now - lastAhSaveMs;
float diffAh = abs(sensors.ahUsed - config.ahUsedSaved);

if (sensors.socResetPending) {
    sensors.socResetPending = false;
    config.ahUsedSaved = sensors.ahUsed; 
    lastAhSaveMs = now;
} 
// CONDIZIONE A: È passato 1 minuto (consumo lento)
// CONDIZIONE B: Abbiamo consumato più di 0.05Ah (consumo veloce, es. Inverter)
else if (elapsed >= AH_SAVE_INTERVAL || diffAh > 0.05f) {
    
    // Salva solo se c'è stata una variazione minima (es. 0.01) per non scrivere a vuoto
    if (diffAh > 0.01f) {
        config.saveAhUsed(sensors.ahUsed);
        config.ahUsedSaved = sensors.ahUsed;
        lastAhSaveMs = now;
        Serial.printf("[System] Save: %.2f Ah (Motivo: %s)\n", 
                      sensors.ahUsed, (diffAh > 0.05f) ? "CARICO ALTO" : "TIMER");
    }
}

  // --- 5. RICONNESSIONE WIFI (Migliorata) ---
  if (config.wifiSTA_SSID.length() > 0 && 
      WiFi.status() != WL_CONNECTED && 
      WiFi.status() != WL_DISCONNECTED && // Evita di sovrapporsi a un tentativo in corso
      now - lastWifiRetryMs >= WIFI_RETRY_MS) {
    
    lastWifiRetryMs = now;
    Serial.println("[WiFi] STA persa, tentativo riconnessione...");
    
    // Non chiamare disconnect(true) o cancelleresti la config AP
    WiFi.begin(config.wifiSTA_SSID.c_str(), config.wifiSTA_Password.c_str());
  }

  delay(10);
}