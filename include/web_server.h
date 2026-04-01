// ============================================================
//  web_server.h - Web Server + API + OTA + OLED
// ============================================================
#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class WebServerManager {
public:
  void init();
  void setupAPI();
  void setupOTA();
  void setupStatic();
private:
  AsyncWebServer httpServer{80};

};

extern WebServerManager webServer;

