// ============================================================
//  web_server.h - Web Server + API + OTA
// ============================================================
#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

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
