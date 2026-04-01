#include <Arduino.h>
#include <new>

#include "config/config_manager.h"
#include "core/boot.h"
#include "diagnostics/log.h"
#include "net/wifi_manager.h"
#include "project_config.h"
#include "protocol/protocol_compat.h"
#include "runtime/scheduler.h"
#include "web/web_ui.h"

#if defined(ZO_DISPLAY_SHARP_MIP)
#include "display/sharp_mip_display.h"
#elif defined(ZO_DISPLAY_ILI9488)
#include "display/ili9488_display.h"
#else
#include "display/st7789_display.h"
#endif

namespace {

zivyobraz::config::ConfigManager* gConfig = nullptr;
#if defined(ZO_DISPLAY_SHARP_MIP)
zivyobraz::display::SharpMipDisplay* gDisplay = nullptr;
#elif defined(ZO_DISPLAY_ILI9488)
zivyobraz::display::Ili9488Display* gDisplay = nullptr;
#else
zivyobraz::display::St7789Display* gDisplay = nullptr;
#endif
zivyobraz::net::WifiManager* gWifi = nullptr;
zivyobraz::protocol::ProtocolCompatService* gProtocol = nullptr;
zivyobraz::runtime::Scheduler* gScheduler = nullptr;
zivyobraz::web::WebUI* gWebUI = nullptr;
bool gInitOk = false;
uint32_t gLastDisplayUpdateMs = 0;

}  // namespace

void setup() {
  zivyobraz::core::initSerial();
  try {
    gConfig = new (std::nothrow) zivyobraz::config::ConfigManager();
    gDisplay = new (std::nothrow)
#if defined(ZO_DISPLAY_SHARP_MIP)
        zivyobraz::display::SharpMipDisplay();
#elif defined(ZO_DISPLAY_ILI9488)
        zivyobraz::display::Ili9488Display();
#else
        zivyobraz::display::St7789Display();
#endif
    gWifi = new (std::nothrow) zivyobraz::net::WifiManager();
    gProtocol = new (std::nothrow) zivyobraz::protocol::ProtocolCompatService();
    gScheduler = new (std::nothrow) zivyobraz::runtime::Scheduler();
    gWebUI = new (std::nothrow) zivyobraz::web::WebUI();

    if (gConfig == nullptr || gDisplay == nullptr || gWifi == nullptr || gProtocol == nullptr ||
        gScheduler == nullptr || gWebUI == nullptr) {
      ZO_LOGE("Setup allocation failed (cfg=%d disp=%d wifi=%d proto=%d sched=%d web=%d)",
              gConfig != nullptr ? 1 : 0, gDisplay != nullptr ? 1 : 0, gWifi != nullptr ? 1 : 0,
              gProtocol != nullptr ? 1 : 0, gScheduler != nullptr ? 1 : 0,
              gWebUI != nullptr ? 1 : 0);
      return;
    }

    gConfig->begin();
    const auto& cfg = gConfig->get();

    zivyobraz::core::logBootBanner(cfg.boardName);

    gWifi->begin(cfg.wifi, cfg.boardName);
    gProtocol->begin(cfg);

    const bool displayOk = gDisplay->begin(cfg.display);
    if (!displayOk) {
      ZO_LOGE("Display init failed");
    } else {
      gDisplay->drawStatusScreen(gScheduler->statusSnapshot());
    }

    gScheduler->begin(gConfig, gWifi, gProtocol, gDisplay);
    gWebUI->begin(gConfig, gWifi);
    gScheduler->setWebUI(gWebUI);

    gInitOk = true;
    ZO_LOGI("Setup complete");
  } catch (const std::bad_alloc&) {
    ZO_LOGE("Setup failed: std::bad_alloc");
  } catch (...) {
    ZO_LOGE("Setup failed: unknown C++ exception");
  }
}

void loop() {
  if (!gInitOk || gScheduler == nullptr || gWebUI == nullptr || gDisplay == nullptr) {
    delay(1000);
    return;
  }

  try {
    gScheduler->tick();
  } catch (const std::bad_alloc&) {
    ZO_LOGE("Loop: std::bad_alloc from scheduler tick (heap free=%u min=%u)",
            static_cast<unsigned int>(ESP.getFreeHeap()),
            static_cast<unsigned int>(ESP.getMinFreeHeap()));
  } catch (...) {
    ZO_LOGE("Loop: unknown C++ exception from scheduler tick");
  }

  try {
    gWebUI->tick();
  } catch (const std::bad_alloc&) {
    ZO_LOGE("Loop: std::bad_alloc from web UI tick (heap free=%u min=%u)",
            static_cast<unsigned int>(ESP.getFreeHeap()),
            static_cast<unsigned int>(ESP.getMinFreeHeap()));
  } catch (...) {
    ZO_LOGE("Loop: unknown C++ exception from web UI tick");
  }

  const uint32_t now = millis();
  if (now - gLastDisplayUpdateMs > 3000) {
    gLastDisplayUpdateMs = now;
    // Only show status screen if we don't have a valid image or in case of errors
    try {
      if (!gScheduler->hasValidImage()) {
        gDisplay->drawStatusScreen(gScheduler->statusSnapshot());
      }
    } catch (const std::bad_alloc&) {
      ZO_LOGE("Loop: std::bad_alloc during status-screen update");
    } catch (...) {
      ZO_LOGE("Loop: unknown C++ exception during status-screen update");
    }
  }

  delay(10);
}
