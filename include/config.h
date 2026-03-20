// ============================================================
//  config.h - Configurazione persistente ESP32
// ============================================================
#pragma once
#include <Arduino.h>
#include <Preferences.h>

#define BAT_TYPE_COUNT 5

struct BatteryProfile {
  const char* name;
  float coulombEff;
  float peukertExp;
};

static const BatteryProfile PROFILES[BAT_TYPE_COUNT] = {
  {"AGM",     0.85f, 1.25f},
  {"GEL",     0.85f, 1.20f},
  {"Pb-open", 0.80f, 1.30f},
  {"LiFePO4", 0.99f, 1.00f},
  {"Li-Ion",  0.98f, 1.00f}
};

class Config {
public:
  // Batteria
  float vHighThreshold    = 14.8f;
  float vLowThreshold     = 11.5f;
  float iHighThreshold    = 100.0f;
  float batteryCapacityAh = 100.0f;
  uint8_t batteryType     = 0;
  float shuntOhm          = 0.00025f;
  float maxCurrentA        = 200.0f;

  // Serbatoi
  uint16_t tankBlackThreshold = 750;
  uint16_t tankGrayThreshold  = 940;
  bool enableTankMonitoring   = true;

  // GPIO
  int pinI2C_SDA    = 21;
  int pinI2C_SCL    = 22;
  int pinTankBlack  = 35;
  int pinTankGray1  = 34;
  int pinTankGray2  = 39;
  int pinTankGray3  = 36;
  int pinTankExcite = 14;

  // WiFi
  bool wifiAPMode       = true;
  String wifiSSID       = "Camper-Monitor";
  String wifiPassword   = "12345678";
  String wifiSTA_SSID   = "";
  String wifiSTA_Password = "";

  // Sistema
  bool debugSerial = true;

  // Persistenti
  float voltageMinSeen =  100.0f;
  float voltageMaxSeen =    0.0f;
  float currentMinSeen = 1000.0f;
  float currentMaxSeen =    0.0f;
  float ahUsedSaved    =    0.0f;

  void load();
  void save();
  void saveMinMax(float vmin, float vmax, float imin, float imax);
  void saveAhUsed(float ah);

private:
  Preferences prefs;
};

extern Config config;
