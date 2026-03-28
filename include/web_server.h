// ============================================================
//  web_server.h - Web Server + API + OTA + OLED
// ============================================================
#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_SSD1306.h>

class WebServerManager {
public:
  void init();
private:
  AsyncWebServer httpServer{80};
  void setupAPI();
  void setupOTA();
  void setupStatic();
};

extern WebServerManager webServer;

// ============================================================
//  OLED Dashboard Functions (in web_server.cpp)
// ============================================================
extern Adafruit_SSD1306 oledDisplay;

void initOLED();
void updateOLED();
void drawOLEDBatteryBar(float soc, int x, int y);
void drawOLEDTankBar(uint8_t tankLevel, bool isGray, int x, int y);
void drawOLEDWiFiBars(int x, int y);
void drawUpArrow(int x, int y);
void drawDownArrow(int x, int y);