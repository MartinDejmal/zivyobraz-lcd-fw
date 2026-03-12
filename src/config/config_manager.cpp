#include "config_manager.h"

#include <Preferences.h>

#include "diagnostics/log.h"

namespace zivyobraz::config {

namespace {
constexpr const char* kPrefsNs = "zo_cfg";
constexpr const char* kKeyWifiSsid = "wifi_ssid";
constexpr const char* kKeyWifiPass = "wifi_pass";
constexpr const char* kKeyHost = "host";
constexpr const char* kKeyUseHttps = "https";
constexpr const char* kKeyApiKey = "api_key";
constexpr const char* kKeyLastTs = "last_ts";
}

void ConfigManager::begin() {
  loadDefaults();
  load();

  if (!isValidApiKey(cfg_.apiKey)) {
    cfg_.apiKey = generateApiKey();
    ZO_LOGW("API key invalid or missing after load; regenerated: %s", cfg_.apiKey.c_str());
    save();
  }
}

const RuntimeConfig& ConfigManager::get() const {
  return cfg_;
}

bool ConfigManager::load() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNs, true)) {
    ZO_LOGW("ConfigManager::load() NVS begin failed, using defaults");
    return false;
  }

  cfg_.wifi.ssid = prefs.getString(kKeyWifiSsid, cfg_.wifi.ssid);
  cfg_.wifi.password = prefs.getString(kKeyWifiPass, cfg_.wifi.password);
  cfg_.server.host = prefs.getString(kKeyHost, cfg_.server.host);
  cfg_.server.useHttps = prefs.getBool(kKeyUseHttps, cfg_.server.useHttps);
  cfg_.apiKey = prefs.getString(kKeyApiKey, "");
  // lastTimestamp is intentionally not loaded from NVS — after every reset the device
  // always fetches fresh image data on the first sync cycle.
  prefs.end();

  if (isValidApiKey(cfg_.apiKey)) {
    ZO_LOGI("API key loaded from NVS: %s", cfg_.apiKey.c_str());
  } else {
    cfg_.apiKey = generateApiKey();
    ZO_LOGW("API key missing/invalid in NVS; regenerated: %s", cfg_.apiKey.c_str());
    save();
  }

  ZO_LOGI("Config loaded: host=%s https=%d ssid=%s ts=%s", cfg_.server.host.c_str(),
          cfg_.server.useHttps ? 1 : 0, cfg_.wifi.ssid.c_str(), cfg_.lastTimestamp.c_str());
  return true;
}

bool ConfigManager::save() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNs, false)) {
    ZO_LOGE("ConfigManager::save() NVS begin failed");
    return false;
  }

  bool ok = true;
  ok &= prefs.putString(kKeyWifiSsid, cfg_.wifi.ssid) == cfg_.wifi.ssid.length();
  ok &= prefs.putString(kKeyWifiPass, cfg_.wifi.password) == cfg_.wifi.password.length();
  ok &= prefs.putString(kKeyHost, cfg_.server.host) == cfg_.server.host.length();
  ok &= prefs.putBool(kKeyUseHttps, cfg_.server.useHttps);
  ok &= prefs.putString(kKeyApiKey, cfg_.apiKey) == cfg_.apiKey.length();
  ok &= prefs.putString(kKeyLastTs, cfg_.lastTimestamp) == cfg_.lastTimestamp.length();

  prefs.end();
  if (!ok) {
    ZO_LOGW("ConfigManager::save() completed with one or more write failures");
  }
  return ok;
}

const String& ConfigManager::apiKey() const {
  return cfg_.apiKey;
}

const String& ConfigManager::lastTimestamp() const {
  return cfg_.lastTimestamp;
}

void ConfigManager::setLastTimestamp(const String& timestamp) {
  cfg_.lastTimestamp = timestamp;
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

  cfg_.wifi.ssid = "";
  cfg_.wifi.password = "";

  cfg_.server.host = "cdn.zivyobraz.eu";
  cfg_.server.useHttps = true;
  cfg_.server.endpointPath = "/index.php";

  cfg_.versions.fwVersion = ZO_FW_VERSION;
  cfg_.versions.apiVersion = ZO_API_VERSION;
  cfg_.apiKey = "";
  cfg_.lastTimestamp = "";
#if defined(PROTOCOL_WIRE_DEBUG) && (PROTOCOL_WIRE_DEBUG == 1)
  cfg_.protocolDebug.wireDebug = true;
#else
  cfg_.protocolDebug.wireDebug = false;
#endif
  cfg_.protocolDebug.forceTimestampCheckZeroOnMissingTimestamp = false;
}

bool ConfigManager::isValidApiKey(const String& apiKey) const {
  if (apiKey.length() != 8) {
    return false;
  }
  for (size_t i = 0; i < apiKey.length(); ++i) {
    if (!isDigit(apiKey[i])) {
      return false;
    }
  }
  return true;
}

String ConfigManager::generateApiKey() const {
  // Random 8-digit numeric key. Leading zeros are preserved.
  uint32_t value = esp_random() % 100000000UL;
  char buf[9] = {0};
  snprintf(buf, sizeof(buf), "%08lu", static_cast<unsigned long>(value));
  return String(buf);
}

}  // namespace zivyobraz::config
