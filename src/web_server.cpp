// ============================================================
//  web_server.cpp - API REST + Config + OTA Web
// ============================================================
#include "web_server.h"
#include "sensors.h"
#include "config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <Update.h>

WebServerManager webServer;

void WebServerManager::init() {
  setupAPI();
  setupOTA();
  setupStatic();
  httpServer.begin();
  Serial.println("[WebServer] Avviato porta 80");
}

void WebServerManager::setupAPI() {

  // --- Dati real-time ---
  httpServer.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *req) {
    StaticJsonDocument<768> doc;
    doc["v"]    = sensors.voltageBus;
    doc["i"]    = sensors.currentA;
    doc["w"]    = sensors.powerW;
    doc["soc"]   = sensors.batterySOC;   // SOC fuso
    doc["socV"]  = sensors.socV;          // SOC da tensione
    doc["socAh"] = sensors.socAh;         // SOC da coulomb counter
    doc["ah"]   = sensors.ahUsed;
    doc["cap"]  = sensors.batteryCapacityAh;

    if (sensors.currentA > 0.1f)       doc["state"] = "Charge";
    else if (sensors.currentA < -0.1f) doc["state"] = "Discharge";
    else                                doc["state"] = "Idle";

    // ETA
    char eta[16] = "--:--";
    if (sensors.currentA < -0.1f) {
      float h = (sensors.batteryCapacityAh - sensors.ahUsed) / fabsf(sensors.currentA);
      snprintf(eta, 16, "%dh%02dm", (int)h, (int)((h - (int)h) * 60));
    } else if (sensors.currentA > 0.1f) {
      float h = sensors.ahUsed / sensors.currentA;
      snprintf(eta, 16, "%dh%02dm", (int)h, (int)((h - (int)h) * 60));
    }
    doc["eta"] = eta;

    doc["vmin"] = sensors.voltageMin;
    doc["vmax"] = sensors.voltageMax;
    doc["imin"] = sensors.currentMin;
    doc["imax"] = sensors.currentMax;

    // Soglie: usa valori config se personalizzati, altrimenti deriva da profilo
    const BatteryProfile& bp = PROFILES[config.batteryType];
    float vHigh = (config.vHighThreshold > 0) ? config.vHighThreshold : bp.fullV + 0.2f;
    float vLow  = (config.vLowThreshold  > 0) ? config.vLowThreshold  : bp.emptyV;
    doc["alm_ovr"] = (sensors.voltageBus > vHigh) ? 1 : 0;
    doc["alm_low"] = (sensors.voltageBus < vLow && sensors.voltageBus > 1.0f) ? 1 : 0;
    doc["alm_cur"] = (fabsf(sensors.currentA) > config.iHighThreshold) ? 1 : 0;
    doc["v_high"]  = vHigh;
    doc["v_low"]   = vLow;

    doc["tank_gray"]  = sensors.tankGray;
    doc["tank_black"] = sensors.tankBlack;
    doc["adc_blk"]    = sensors.tankADC[0];
    doc["adc_g1"]     = sensors.tankADC[1];
    doc["adc_g2"]     = sensors.tankADC[2];
    doc["adc_g3"]     = sensors.tankADC[3];

    doc["sim"]       = sensors.simMode ? 1 : 0;
    doc["connected"] = sensors.inaPresent ? 1 : 0;
    doc["rssi"]      = WiFi.RSSI();
    doc["uptime"]    = millis() / 1000;
    doc["heap"]      = ESP.getFreeHeap();

    bool staOk = (WiFi.status() == WL_CONNECTED);
    doc["sta"]      = staOk ? 1 : 0;
    doc["sta_ip"]   = staOk ? WiFi.localIP().toString() : String("");
    doc["ap_ip"]    = WiFi.softAPIP().toString();
    doc["wifiMode"] = "AP+STA";

    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
  });

  // --- Storico per grafico ---
  httpServer.on("/api/history", HTTP_GET, [](AsyncWebServerRequest *req) {
    int count = sensors.histCount;
    int start = (count < SensorManager::HIST_SIZE) ? 0 : sensors.histHead;
    // Formato {"voltage":[{"v":4.62},...], "current":[{"v":0.1},...]}
    String json = "{";
    json += "\"voltage\":[";
    for (int n = 0; n < count; n++) {
      int idx = (start + n) % SensorManager::HIST_SIZE;
      if (n > 0) json += ",";
      json += "{\"v\":" + String(sensors.history[idx].v, 2) + "}";
    }
    json += "],\"current\":[";
    for (int n = 0; n < count; n++) {
      int idx = (start + n) % SensorManager::HIST_SIZE;
      if (n > 0) json += ",";
      json += "{\"v\":" + String(sensors.history[idx].i, 1) + "}";
    }
    json += "]}";
    req->send(200, "application/json", json);
  });

  // --- Reset min/max ---
  httpServer.on("/api/reset-minmax", HTTP_POST, [](AsyncWebServerRequest *req) {
    sensors.resetMinMax();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // --- Leggi configurazione ---
  httpServer.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *req) {
    StaticJsonDocument<1024> doc;
    doc["vHigh"]     = config.vHighThreshold;
    doc["vLow"]      = config.vLowThreshold;
    doc["iHigh"]     = config.iHighThreshold;
    doc["capAh"]     = config.batteryCapacityAh;
    doc["batType"]   = config.batteryType;
    doc["shuntOhm"]  = config.shuntOhm;
    doc["maxI"]      = config.maxCurrentA;
    doc["nomV"]      = config.nominalVoltage;
    doc["iScale"]    = config.currentScale;

    doc["tBlkThr"]   = config.tankBlackThreshold;
    doc["tGryThr"]   = config.tankGrayThreshold;
    doc["tankEn"]    = config.enableTankMonitoring;

    doc["pSDA"]  = config.pinI2C_SDA;
    doc["pSCL"]  = config.pinI2C_SCL;
    doc["pTBlk"] = config.pinTankBlack;
    doc["pTG1"]  = config.pinTankGray1;
    doc["pTG2"]  = config.pinTankGray2;
    doc["pTG3"]  = config.pinTankGray3;
    doc["pExc"]  = config.pinTankExcite;

    doc["wifiAP"]    = config.wifiAPMode;
    doc["wSSID"]     = config.wifiSSID;
    doc["wPass"]     = config.wifiPassword;
    doc["wStaSSID"]  = config.wifiSTA_SSID;
    doc["wStaPass"]  = config.wifiSTA_Password;

    // Nomi profili batteria con tensioni
    JsonArray bats = doc.createNestedArray("batTypes");
    for (int i = 0; i < BAT_TYPE_COUNT; i++) bats.add(PROFILES[i].name);
    JsonArray batNomV = doc.createNestedArray("batNomV");
    for (int i = 0; i < BAT_TYPE_COUNT; i++) batNomV.add(PROFILES[i].nominalV);

    // ADC live per calibrazione
    doc["adc_blk"] = sensors.tankADC[0];
    doc["adc_g1"]  = sensors.tankADC[1];
    doc["adc_g2"]  = sensors.tankADC[2];
    doc["adc_g3"]  = sensors.tankADC[3];

    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
  });

  // --- Salva configurazione ---
  httpServer.on("/api/config", HTTP_POST,
    [](AsyncWebServerRequest *req) {},
    NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<1024> doc;
      if (deserializeJson(doc, data, len)) {
        req->send(400, "application/json", "{\"error\":\"JSON non valido\"}");
        return;
      }

      bool needReboot = false;

      if (doc.containsKey("vHigh"))    config.vHighThreshold    = doc["vHigh"];
      if (doc.containsKey("vLow"))     config.vLowThreshold     = doc["vLow"];
      if (doc.containsKey("iHigh"))    config.iHighThreshold    = doc["iHigh"];
      if (doc.containsKey("capAh"))    { config.batteryCapacityAh = doc["capAh"]; sensors.batteryCapacityAh = config.batteryCapacityAh; }
      if (doc.containsKey("batType"))  config.batteryType       = doc["batType"];
      if (doc.containsKey("shuntOhm")) { config.shuntOhm = doc["shuntOhm"]; sensors.reInitINA(); }
      if (doc.containsKey("maxI"))     { config.maxCurrentA = doc["maxI"]; sensors.reInitINA(); }
      if (doc.containsKey("nomV"))     config.nominalVoltage = doc["nomV"];
      if (doc.containsKey("iScale"))   config.currentScale   = doc["iScale"];

      if (doc.containsKey("tBlkThr")) config.tankBlackThreshold = doc["tBlkThr"];
      if (doc.containsKey("tGryThr")) config.tankGrayThreshold  = doc["tGryThr"];
      if (doc.containsKey("tankEn"))  config.enableTankMonitoring = doc["tankEn"];

      // GPIO — richiede reboot
      if (doc.containsKey("pSDA"))  { config.pinI2C_SDA    = doc["pSDA"];  needReboot = true; }
      if (doc.containsKey("pSCL"))  { config.pinI2C_SCL    = doc["pSCL"];  needReboot = true; }
      if (doc.containsKey("pTBlk")) { config.pinTankBlack  = doc["pTBlk"]; needReboot = true; }
      if (doc.containsKey("pTG1"))  { config.pinTankGray1  = doc["pTG1"];  needReboot = true; }
      if (doc.containsKey("pTG2"))  { config.pinTankGray2  = doc["pTG2"];  needReboot = true; }
      if (doc.containsKey("pTG3"))  { config.pinTankGray3  = doc["pTG3"];  needReboot = true; }
      if (doc.containsKey("pExc"))  { config.pinTankExcite = doc["pExc"];  needReboot = true; }

      // WiFi — richiede reboot
      if (doc.containsKey("wifiAP"))   { config.wifiAPMode       = doc["wifiAP"];   needReboot = true; }
      if (doc.containsKey("wSSID"))    { config.wifiSSID         = doc["wSSID"].as<String>();    needReboot = true; }
      if (doc.containsKey("wPass"))    { config.wifiPassword     = doc["wPass"].as<String>();    needReboot = true; }
      if (doc.containsKey("wStaSSID")) { config.wifiSTA_SSID     = doc["wStaSSID"].as<String>(); needReboot = true; }
      if (doc.containsKey("wStaPass")) { config.wifiSTA_Password = doc["wStaPass"].as<String>(); needReboot = true; }

      config.save();

      StaticJsonDocument<64> resp;
      resp["ok"] = true;
      resp["reboot"] = needReboot;
      String json;
      serializeJson(resp, json);
      req->send(200, "application/json", json);

      if (needReboot) {
        delay(500);
        ESP.restart();
      }
    }
  );

  // --- Reboot ---
  httpServer.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *req) {
    req->send(200, "application/json", "{\"ok\":true}");
    delay(500);
    ESP.restart();
  });
  
    // --- Scansione WiFi ---
  httpServer.on("/api/wifi-scan", HTTP_GET, [](AsyncWebServerRequest *req) {
    // Nota: La scansione viene eseguita in modo sincrono qui per semplicità 
    // poiché restituisce i risultati immediatamente dopo la fine della scansione.
    int n = WiFi.scanNetworks();
    
    String json = "[";
    for (int i = 0; i < n; ++i) {
      if (i > 0) json += ",";
      json += "{";
      json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
      json += "\"rssi\":" + String(WiFi.RSSI(i));
      json += "}";
    }
    json += "]";
    
    WiFi.scanDelete(); // Libera memoria
    req->send(200, "application/json", json);
  });
  
}


// === OTA via Web ===
void WebServerManager::setupOTA() {

  // Upload firmware (.bin)
  httpServer.on("/api/ota/firmware", HTTP_POST,
    [](AsyncWebServerRequest *req) {
      bool ok = !Update.hasError();
      req->send(200, "application/json",
        ok ? "{\"ok\":true,\"msg\":\"Firmware aggiornato, riavvio...\"}"
           : "{\"ok\":false,\"msg\":\"Errore upload firmware\"}");
      if (ok) { delay(500); ESP.restart(); }
    },
    [](AsyncWebServerRequest *req, const String& filename, size_t index,
       uint8_t *data, size_t len, bool final) {
      if (!index) {
        Serial.printf("[OTA] Firmware: %s (%u bytes)\n", filename.c_str(), req->contentLength());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
          Update.printError(Serial);
        }
      }
      if (Update.isRunning()) {
        if (Update.write(data, len) != len) Update.printError(Serial);
      }
      if (final) {
        if (!Update.end(true)) Update.printError(Serial);
        else Serial.println("[OTA] Firmware OK");
      }
    }
  );

  // Upload filesystem (.bin)
  httpServer.on("/api/ota/filesystem", HTTP_POST,
    [](AsyncWebServerRequest *req) {
      bool ok = !Update.hasError();
      req->send(200, "application/json",
        ok ? "{\"ok\":true,\"msg\":\"Filesystem aggiornato, riavvio...\"}"
           : "{\"ok\":false,\"msg\":\"Errore upload filesystem\"}");
      if (ok) { delay(500); ESP.restart(); }
    },
    [](AsyncWebServerRequest *req, const String& filename, size_t index,
       uint8_t *data, size_t len, bool final) {
      if (!index) {
        Serial.printf("[OTA] Filesystem: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
          Update.printError(Serial);
        }
      }
      if (Update.isRunning()) {
        if (Update.write(data, len) != len) Update.printError(Serial);
      }
      if (final) {
        if (!Update.end(true)) Update.printError(Serial);
        else Serial.println("[OTA] Filesystem OK");
      }
    }
  );
}

void WebServerManager::setupStatic() {
  httpServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  httpServer.onNotFound([](AsyncWebServerRequest *req) {
    req->send(404, "text/plain", "Not Found");
  });
}