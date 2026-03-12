#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "config/config_manager.h"
#include "image/indexed_framebuffer.h"
#include "net/wifi_manager.h"

namespace zivyobraz::web {

class WebUI {
 public:
  // Call once during setup. Starts the HTTP server on port 80.
  void begin(config::ConfigManager* config, net::WifiManager* wifi);

  // Call from the main loop; dispatches pending HTTP requests.
  void tick();

  // Called by the Scheduler after a successful render so that the image
  // preview endpoint can serve a fresh frame.
  void updatePreview(const image::IndexedFramebuffer& framebuffer);

 private:
  // Route handlers
  void handleRoot();
  void handleApiStatus();
  void handleApiPreview();
  void handleApiLog();
  void handleApiConfig();
  void handleApiRestart();
  void handleApiWifiScan();
  void handleGenerate204();
  void handleHotspotDetect();
  void handleConnectTest();
  void handleCaptiveRedirect();

  // Helpers
  static void colorIndexToRgb(uint8_t index, uint8_t& r, uint8_t& g, uint8_t& b);
  static String formatUptime();
  static String escapeJson(const String& s);

  WebServer server_{80};
  config::ConfigManager* config_{nullptr};
  net::WifiManager* wifi_{nullptr};
  image::IndexedFramebuffer lastFramebuffer_;
  bool hasPreview_{false};
};

}  // namespace zivyobraz::web
