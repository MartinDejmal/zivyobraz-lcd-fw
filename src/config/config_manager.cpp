#include "config_manager.h"

#include "diagnostics/log.h"

namespace zivyobraz::config {

void ConfigManager::begin() {
  loadDefaults();
  load();
}

const RuntimeConfig& ConfigManager::get() const {
  return cfg_;
}

bool ConfigManager::load() {
  // TODO: Replace with NVS-backed persistence.
  ZO_LOGI("ConfigManager::load() stub -> defaults active");
  return true;
}

bool ConfigManager::save() {
  // TODO: Replace with NVS-backed persistence.
  ZO_LOGI("ConfigManager::save() stub");
  return true;
}

void ConfigManager::loadDefaults() {
  cfg_.boardName = "esp32dev";
  cfg_.display.type = DisplayType::St7789;
  cfg_.display.width = 240;
  cfg_.display.height = 240;
  cfg_.display.rotation = 0;
  cfg_.display.pins = {
      .mosi = 23,
      .sclk = 18,
      .cs = -1,
      .dc = 16,
      .rst = 17,
      .bl = 4,
  };

  cfg_.wifi.ssid = "TODO_WIFI_SSID";
  cfg_.wifi.password = "TODO_WIFI_PASSWORD";

  cfg_.server.host = "cdn.zivyobraz.eu";
  cfg_.server.useHttps = true;
  cfg_.server.endpointPath = "/index.php";

  cfg_.versions.fwVersion = ZO_FW_VERSION;
  cfg_.versions.apiVersion = ZO_API_VERSION;
}

}  // namespace zivyobraz::config
