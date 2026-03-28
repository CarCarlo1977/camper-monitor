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
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>


WebServerManager webServer;

// ============================================================
//  OLED Dashboard GERUI (Verde su Nero) - Integrato
// ============================================================
Adafruit_SSD1306 oledDisplay(128, 64, &Wire, -1);

static unsigned long oledLastUpdate = 0;
static unsigned long oledBlinkMs = 0;
static bool oledBlinkState = false;

// Funzioni forward declaration
void initOLED();
void updateOLED();
void drawOLEDBatteryBar(float soc, int x, int y);
void drawOLEDTankBar(uint8_t tankLevel, bool isGray, int x, int y);
void drawOLEDWiFiBars(int x, int y);
void initOLED() {
  Serial.println("[OLED] Initializing SSD1306 1.3\" at 0x3C...");
  
  if (!oledDisplay.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[OLED] ERROR: Display not found!");
    return;
  }

  delay(100);
  oledDisplay.clearDisplay();
  delay(100);
  

  oledDisplay.dim(false);              // Full brightness
  
  oledDisplay.invertDisplay(true);
  delay(500);
  oledDisplay.invertDisplay(false);
  
  Serial.println("[OLED] ✓ Display found at 0x3C (128x64 pixels)");
  
  // === SPLASH SCREEN ===
  oledDisplay.clearDisplay();
  oledDisplay.setTextSize(2);
  oledDisplay.setTextColor(SSD1306_WHITE);
  oledDisplay.setCursor(10, 0);
  oledDisplay.println("CAMPER");
  oledDisplay.setCursor(15, 16);
  oledDisplay.println("MONITOR");
  
  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(20, 35);
  oledDisplay.println("v3.0");
  
  oledDisplay.setCursor(8, 48);
  oledDisplay.println("Initializing...");
  
  oledDisplay.display();
  delay(2000);
  
  // === WELCOME SCREEN ===
  oledDisplay.clearDisplay();
  oledDisplay.setTextSize(1);
  oledDisplay.setTextColor(SSD1306_WHITE);
  
  oledDisplay.setCursor(2, 0);
  oledDisplay.println("=== READY ===");
  oledDisplay.setCursor(2, 10);
  oledDisplay.println("OLED: OK");
  oledDisplay.setCursor(2, 18);
  oledDisplay.println("INA228: OK");
  oledDisplay.setCursor(2, 26);
  oledDisplay.println("WiFi: Connected");
  oledDisplay.setCursor(2, 34);
  oledDisplay.println("LCD: OK");
  oledDisplay.setCursor(2, 42);
  oledDisplay.println("Encoder: OK");
  oledDisplay.setCursor(2, 50);
  oledDisplay.println("System READY!");
  
  oledDisplay.display();
  delay(3000);
  
  Serial.println("[OLED] ✓ OLED Dashboard READY!");
}

void updateOLED() {
  unsigned long now = millis();
  
  // Update OLED ogni 1 secondo
  if (now - oledLastUpdate < 1000) return;
  oledLastUpdate = now;
  
  // Aggiorna blink state (lampeggia ogni 500ms)
  if (now - oledBlinkMs >= 500) {
    oledBlinkMs = now;
    oledBlinkState = !oledBlinkState;
  }
  
  oledDisplay.clearDisplay();
  oledDisplay.setTextColor(SSD1306_WHITE);
  
  // ===== ROW 1: Voltage + Power + WiFi + CHG/DCH status (ALL ON FIRST ROW!) =====
  oledDisplay.setTextSize(3);
  oledDisplay.setCursor(0, 0);
  oledDisplay.printf("%.1fV", sensors.voltageBus);
  
  // Power (size 3, center)
  oledDisplay.setCursor(42, 0);
  oledDisplay.printf("%.0fW", sensors.powerW);
  
  // WiFi indicator (right)
  drawOLEDWiFiBars(100, 0);
  
  // Status with animated arrow - DRAWN with lines (not font)
  oledDisplay.setTextSize(1);
  if (sensors.currentA < -0.05f) {
    // Charging: freccia in su (↑) disegnata e lampeggiante
    if (oledBlinkState) {
      drawUpArrow(100, 4);
    }
  } else if (sensors.currentA > 0.05f) {
    // Discharging: freccia in giu (↓) disegnata e lampeggiante
    if (oledBlinkState) {
      drawDownArrow(100, 4);
    }
  } else {
    // Idle: no arrow
    oledDisplay.setCursor(95, 6);
    oledDisplay.print("IDLE");
  }
  

  // ===== ROW 2: Current & Ah =====
  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(0, 22);
  oledDisplay.printf("I: %.2fA", sensors.currentA);
  
  oledDisplay.setCursor(60, 22);
  oledDisplay.printf("Ah:%.1f", sensors.ahUsed);
  
  // ===== ROW 3: SOC (HUGE size 3) =====
  oledDisplay.setTextSize(3);
  oledDisplay.setCursor(0, 28);
  int socInt = constrain((int)sensors.batterySOC, 0, 100);
  if (socInt < 10) oledDisplay.print(" ");
  oledDisplay.printf("%d%%", socInt);
  
  // ===== ROW 4: Battery Bar =====
  drawOLEDBatteryBar(sensors.batterySOC, 0, 44);
  
  // ===== DIVIDER LINE =====
  oledDisplay.drawLine(0, 51, 127, 51, SSD1306_WHITE);
  
  // ===== ROW 5: Tanks (LABEL + BAR) =====
  oledDisplay.setTextSize(1);
  
  // Gray Water Tank (LEFT)
  oledDisplay.setCursor(2, 53);
  oledDisplay.print("GRAY");
  drawOLEDTankBar(sensors.tankGray, true, 2, 59);
  
  // Black Water Tank (RIGHT)
  oledDisplay.setCursor(65, 53);
  oledDisplay.print("BLACK");
  drawOLEDTankBar(sensors.tankBlack, false, 65, 59);
  
  oledDisplay.display();
}

void drawOLEDBatteryBar(float soc, int x, int y) {
  int barWidth = 54;
  int barHeight = 5;
  
  oledDisplay.drawRect(x, y, barWidth, barHeight, SSD1306_WHITE);
  oledDisplay.drawRect(x + barWidth, y + 1, 2, 3, SSD1306_WHITE);
  
  int fillWidth = constrain((int)((barWidth - 2) * soc / 100.0f), 0, barWidth - 2);
  if (fillWidth > 1) {
    oledDisplay.fillRect(x + 1, y + 1, fillWidth, barHeight - 2, SSD1306_WHITE);
  }
  
  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(x + barWidth + 4, y);
  int pct = (int)soc;
  if (pct < 10) oledDisplay.print(" ");
  if (pct < 100) oledDisplay.print(" ");
  oledDisplay.printf("%d%%", pct);
}

void drawOLEDTankBar(uint8_t tankLevel, bool isGray, int x, int y) {
  int barWidth = 25;
  int barHeight = 4;
  
  oledDisplay.drawRect(x, y, barWidth, barHeight, SSD1306_WHITE);
  
  float fillPercent = 0;
  
  if (isGray) {
    // Gray: 0=FULL, 1=2/3, 2=1/3, 3=EMPTY
    if (tankLevel == 0) fillPercent = 1.0f;
    else if (tankLevel == 1) fillPercent = 0.66f;
    else if (tankLevel == 2) fillPercent = 0.33f;
    else fillPercent = 0.0f;
  } else {
    // Black: 0=FULL, 1=OK
    if (tankLevel == 0) fillPercent = 1.0f;
    else fillPercent = 0.5f;
  }
  
  fillPercent = constrain(fillPercent, 0.0f, 1.0f);
  int fillWidth = (int)(barWidth * fillPercent);
  
  if (fillWidth > 2) {
    oledDisplay.fillRect(x + 1, y + 1, fillWidth - 1, barHeight - 2, SSD1306_WHITE);
  }
  
  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(x + 2, y + 5);
  if (isGray) {
    if (tankLevel == 0) oledDisplay.print("FULL");
    else if (tankLevel == 1) oledDisplay.print("2/3");
    else if (tankLevel == 2) oledDisplay.print("1/3");
    else oledDisplay.print("--");
  } else {
    if (tankLevel == 0) oledDisplay.print("FULL");
    else oledDisplay.print("OK");
  }
}

void drawUpArrow(int x, int y) {
  // Freccia in su disegnata con linee
  // Asta verticale (3 pixel)
  oledDisplay.drawLine(x, y + 8, x, y + 2, SSD1306_WHITE);
  
  // Punta sinistra (angolo alto sinistro)
  oledDisplay.drawLine(x, y + 2, x - 3, y + 5, SSD1306_WHITE);
  
  // Punta destra (angolo alto destro)
  oledDisplay.drawLine(x, y + 2, x + 3, y + 5, SSD1306_WHITE);
}

void drawDownArrow(int x, int y) {
  // Freccia in giù disegnata con linee
  // Asta verticale
  oledDisplay.drawLine(x, y, x, y + 6, SSD1306_WHITE);
  
  // Punta sinistra (angolo basso sinistro)
  oledDisplay.drawLine(x, y + 6, x - 3, y + 3, SSD1306_WHITE);
  
  // Punta destra (angolo basso destro)
  oledDisplay.drawLine(x, y + 6, x + 3, y + 3, SSD1306_WHITE);
}
void drawOLEDWiFiBars(int x, int y) {
  // WiFi indicator: 3 bars
  // Bar 1 (short, left)
  oledDisplay.drawRect(x, y + 6, 3, 3, SSD1306_WHITE);
  oledDisplay.fillRect(x, y + 6, 3, 3, SSD1306_WHITE);
  
  // Bar 2 (medium, middle)
  oledDisplay.drawRect(x + 5, y + 3, 3, 6, SSD1306_WHITE);
  oledDisplay.fillRect(x + 5, y + 3, 3, 6, SSD1306_WHITE);
  
  // Bar 3 (tall, right)
  oledDisplay.drawRect(x + 10, y, 3, 9, SSD1306_WHITE);
  oledDisplay.fillRect(x + 10, y, 3, 9, SSD1306_WHITE);
}
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
  // --- Sostituisci la rotta /api/history con questa ---
httpServer.on("/api/history", HTTP_GET, [](AsyncWebServerRequest *req) {
    AsyncResponseStream *response = req->beginResponseStream("application/json");
    
    int count = sensors.histCount;
    int start = (count < SensorManager::HIST_SIZE) ? 0 : sensors.histHead;

    response->print("{\"voltage\":[");
    for (int n = 0; n < count; n++) {
        int idx = (start + n) % SensorManager::HIST_SIZE;
        if (n > 0) response->print(",");
        response->printf("{\"v\":%.2f}", sensors.history[idx].v);
    }

    response->print("],\"current\":[");
    for (int n = 0; n < count; n++) {
        int idx = (start + n) % SensorManager::HIST_SIZE;
        if (n > 0) response->print(",");
        response->printf("{\"v\":%.1f}", sensors.history[idx].i);
    }
    response->print("]}");
    
    req->send(response);
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

// In web_server.cpp - Rotta /api/reset-soc
httpServer.on("/api/reset-soc", HTTP_POST, [](AsyncWebServerRequest *req) {
    // Reset RAM Sensori
    sensors.ahUsed = 0.0f;
    sensors.batterySOC = 100.0f;
    sensors.socAh = 100.0f;
    sensors.socV = 100.0f;
    
    // Reset RAM Config (Il riferimento per il loop)
    config.ahUsedSaved = 0.0f; 
    
    // Scrittura Fisica Flash
    config.saveAhUsed(0.0f);   
    
    // Notifica al loop di non salvare sopra
    sensors.socResetPending = true; 
    
    req->send(200, "application/json", "{\"ok\":true}");
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
        config.forceSaveAh(sensors.ahUsed);
        delay(500);
        ESP.restart();
      }
    }
  );

  // --- Reboot ---
  httpServer.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *req) {
    config.forceSaveAh(sensors.ahUsed);
    req->send(200, "application/json", "{\"ok\":true}");
    delay(500);
    ESP.restart();
  });
  
    // --- Scansione WiFi ---
  // --- Versione ottimizzata di /api/wifi-scan ---
httpServer.on("/api/wifi-scan", HTTP_GET, [](AsyncWebServerRequest *req) {
    int n = WiFi.scanComplete();
    if(n == -2) { // Scansione non avviata
        WiFi.scanNetworks(true);
        req->send(202, "application/json", "{\"msg\":\"Scanning...\"}");
    } else if(n >= 0) { // Scansione completata
        String json = "[";
        for (int i = 0; i < n; ++i) {
            if (i > 0) json += ",";
            json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
        }
        json += "]";
        WiFi.scanDelete();
        req->send(200, "application/json", json);
    } else { // Scansione in corso
        req->send(202, "application/json", "{\"msg\":\"Scanning...\"}");
    }
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