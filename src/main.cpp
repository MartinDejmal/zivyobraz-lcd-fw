#include <Arduino.h>

#include "config/config_manager.h"
#include "core/boot.h"
#include "diagnostics/log.h"
#include "display/st7789_display.h"
#include "net/wifi_manager.h"
#include "project_config.h"
#include "protocol/protocol_compat.h"
#include "runtime/scheduler.h"
#include "web/web_ui.h"

namespace {

zivyobraz::config::ConfigManager gConfig;
zivyobraz::display::St7789Display gDisplay;
zivyobraz::net::WifiManager gWifi;
zivyobraz::protocol::ProtocolCompatService gProtocol;
zivyobraz::runtime::Scheduler gScheduler;
zivyobraz::web::WebUI gWebUI;
uint32_t gLastDisplayUpdateMs = 0;

}  // namespace

void setup() {
  zivyobraz::core::initSerial();

  gConfig.begin();
  const auto& cfg = gConfig.get();

  zivyobraz::core::logBootBanner(cfg.boardName);

  gWifi.begin(cfg.wifi);
  gProtocol.begin(cfg);

  const bool displayOk = gDisplay.begin(cfg.display);
  if (!displayOk) {
    ZO_LOGE("Display init failed");
  } else {
    gDisplay.drawStatusScreen(gScheduler.statusSnapshot());
  }

  gScheduler.begin(&gConfig, &gWifi, &gProtocol, &gDisplay);
  gWebUI.begin(&gConfig, &gWifi);
  gScheduler.setWebUI(&gWebUI);

  ZO_LOGI("Setup complete");
}

void loop() {
  gScheduler.tick();
  gWebUI.tick();

  const uint32_t now = millis();
  if (now - gLastDisplayUpdateMs > 3000) {
    gLastDisplayUpdateMs = now;
    // Only show status screen if we don't have a valid image or in case of errors
    if (!gScheduler.hasValidImage()) {
      gDisplay.drawStatusScreen(gScheduler.statusSnapshot());
    }
  }

  delay(10);
}
