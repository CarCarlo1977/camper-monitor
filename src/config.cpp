// ============================================================
//  config.cpp - Implementazione Config con NVS
// ============================================================
#include "config.h"

Config config;

void Config::load() {
  prefs.begin("camper", true);
  vHighThreshold    = prefs.getFloat("vHigh",    14.8f);
  vLowThreshold     = prefs.getFloat("vLow",     11.5f);
  iHighThreshold    = prefs.getFloat("iHigh",    100.0f);
  batteryCapacityAh = prefs.getFloat("capAh",    100.0f);
  batteryType       = prefs.getUChar("batType",   0);
  shuntOhm          = prefs.getFloat("shuntOhm", 0.00025f);
  maxCurrentA       = prefs.getFloat("maxI",      200.0f);
  nominalVoltage    = prefs.getFloat("nomV",      12.0f);
  currentScale      = prefs.getFloat("iScale",    1.0f);

  tankBlackThreshold  = prefs.getUShort("tBlkThr", 750);
  tankGrayThreshold   = prefs.getUShort("tGryThr", 940);
  enableTankMonitoring = prefs.getBool("tankEn",   true);

  pinI2C_SDA    = prefs.getInt("pSDA",    21);
  pinI2C_SCL    = prefs.getInt("pSCL",    22);
  pinTankBlack  = prefs.getInt("pTBlk",   35);
  pinTankGray1  = prefs.getInt("pTG1",    34);
  pinTankGray2  = prefs.getInt("pTG2",    39);
  pinTankGray3  = prefs.getInt("pTG3",    36);
  pinTankExcite = prefs.getInt("pExc",    14);

  wifiAPMode      = prefs.getBool("wifiAP",    true);
  wifiSSID        = prefs.getString("wSSID",    "Camper-Monitor");
  wifiPassword    = prefs.getString("wPass",    "12345678");
  wifiSTA_SSID    = prefs.getString("wStaSSID", "");
  wifiSTA_Password = prefs.getString("wStaPass","");

  debugSerial = prefs.getBool("dbgSer", true);

  voltageMinSeen = prefs.getFloat("vMin",  100.0f);
  voltageMaxSeen = prefs.getFloat("vMax",    0.0f);
  currentMinSeen = prefs.getFloat("iMin", 1000.0f);
  currentMaxSeen = prefs.getFloat("iMax",    0.0f);
  ahUsedSaved    = prefs.getFloat("ahUsed",  0.0f);

  prefs.end();

  // Validazione
  if (batteryType >= BAT_TYPE_COUNT) batteryType = 0;
  if (batteryCapacityAh <= 0 || batteryCapacityAh > 2000) batteryCapacityAh = 100.0f;
  if (isnan(shuntOhm) || shuntOhm < 0.00001f || shuntOhm > 0.1f) shuntOhm = 0.00025f;
  if (ahUsedSaved < 0 || ahUsedSaved > batteryCapacityAh) ahUsedSaved = 0.0f;
}

void Config::save() {
  prefs.begin("camper", false);
  prefs.putFloat("vHigh",    vHighThreshold);
  prefs.putFloat("vLow",     vLowThreshold);
  prefs.putFloat("iHigh",    iHighThreshold);
  prefs.putFloat("capAh",    batteryCapacityAh);
  prefs.putUChar("batType",  batteryType);
  prefs.putFloat("shuntOhm", shuntOhm);
  prefs.putFloat("maxI",     maxCurrentA);
  prefs.putFloat("nomV",     nominalVoltage);
  prefs.putFloat("iScale",   currentScale);

  prefs.putUShort("tBlkThr", tankBlackThreshold);
  prefs.putUShort("tGryThr", tankGrayThreshold);
  prefs.putBool("tankEn",    enableTankMonitoring);

  prefs.putInt("pSDA",    pinI2C_SDA);
  prefs.putInt("pSCL",    pinI2C_SCL);
  prefs.putInt("pTBlk",   pinTankBlack);
  prefs.putInt("pTG1",    pinTankGray1);
  prefs.putInt("pTG2",    pinTankGray2);
  prefs.putInt("pTG3",    pinTankGray3);
  prefs.putInt("pExc",    pinTankExcite);

  prefs.putBool("wifiAP",    wifiAPMode);
  prefs.putString("wSSID",    wifiSSID);
  prefs.putString("wPass",    wifiPassword);
  prefs.putString("wStaSSID", wifiSTA_SSID);
  prefs.putString("wStaPass", wifiSTA_Password);

  prefs.putBool("dbgSer", debugSerial);
  prefs.end();
}

void Config::saveMinMax(float vmin, float vmax, float imin, float imax) {
  prefs.begin("camper", false);
  prefs.putFloat("vMin", vmin);
  prefs.putFloat("vMax", vmax);
  prefs.putFloat("iMin", imin);
  prefs.putFloat("iMax", imax);
  prefs.end();
}

void Config::saveAhUsed(float ah) {
  prefs.begin("camper", false);
  prefs.putFloat("ahUsed", ah);
  prefs.end();
}