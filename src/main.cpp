#include <Arduino.h>

#include "config/config_manager.h"
#include "core/boot.h"
#include "diagnostics/log.h"
#include "display/st7789_display.h"
#include "net/wifi_manager.h"
#include "project_config.h"
#include "protocol/protocol_compat.h"
#include "runtime/scheduler.h"

namespace {

zivyobraz::config::ConfigManager gConfig;
zivyobraz::display::St7789Display gDisplay;
zivyobraz::net::WifiManager gWifi;
zivyobraz::protocol::ProtocolCompatService gProtocol;
zivyobraz::runtime::Scheduler gScheduler;
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
    gDisplay.drawStatusScreen(cfg.versions.fwVersion, gScheduler.wifiDiagnostics(),
                              gScheduler.protocolDiagnostics());
  }

  gScheduler.begin(&gConfig, &gWifi, &gProtocol);
  ZO_LOGI("Setup complete");
}

void loop() {
  gScheduler.tick();

  const uint32_t now = millis();
  if (now - gLastDisplayUpdateMs > 3000) {
    gLastDisplayUpdateMs = now;
    gDisplay.drawStatusScreen(gConfig.get().versions.fwVersion, gScheduler.wifiDiagnostics(),
                              gScheduler.protocolDiagnostics());
  }

  delay(10);
}
