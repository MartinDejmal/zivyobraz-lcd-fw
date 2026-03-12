#include "wifi_manager.h"

#include <WiFi.h>

#include "diagnostics/log.h"

namespace zivyobraz::net {

namespace {
constexpr uint32_t kReconnectIntervalMs = 10000;
constexpr uint8_t kDnsPort = 53;
constexpr char kDefaultApPassword[] = "zivyobraz-setup";
}

void WifiManager::begin(const WifiConfig& cfg, const String& deviceName) {
  cfg_ = cfg;
  deviceName_ = deviceName;
  apRetries_ = 0;
  portalActive_ = false;
  apSsid_ = buildApSsid();
  apPassword_ = String(kDefaultApPassword);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);

  if (cfg_.ssid.isEmpty()) {
    state_ = WifiState::Unconfigured;
    ZO_LOGW("Wi-Fi config missing (empty SSID), starting captive AP mode");
    startApPortal();
    return;
  }

  state_ = WifiState::Connecting;
  ZO_LOGI("WifiManager initialized, ssid='%s'", cfg_.ssid.c_str());
  if (!connect(12000)) {
    ZO_LOGW("Initial STA connect failed, switching to captive AP mode");
    startApPortal();
  }
}

void WifiManager::tick() {
  if (portalActive_) {
    dnsServer_.processNextRequest();
  }

  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    state_ = WifiState::Connected;
    return;
  }

  if (cfg_.ssid.isEmpty()) {
    state_ = WifiState::Unconfigured;
    startApPortal();
    return;
  }

  const uint32_t now = millis();
  if (state_ == WifiState::ConnectFailed && now - lastReconnectAttemptMs_ < kReconnectIntervalMs) {
    return;
  }

  if (now - lastReconnectAttemptMs_ >= kReconnectIntervalMs) {
    lastReconnectAttemptMs_ = now;
    reconnect();
  }
}

bool WifiManager::connect(uint32_t timeoutMs) {
  if (cfg_.ssid.isEmpty()) {
    state_ = WifiState::Unconfigured;
    return false;
  }

  if (WiFi.status() == WL_CONNECTED) {
    stopApPortal();
    state_ = WifiState::Connected;
    return true;
  }

  state_ = WifiState::Connecting;
  ++apRetries_;
  ZO_LOGI("Wi-Fi connecting attempt #%lu to SSID '%s'", static_cast<unsigned long>(apRetries_),
          cfg_.ssid.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg_.ssid.c_str(), cfg_.password.c_str());

  const uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      stopApPortal();
      state_ = WifiState::Connected;
      ZO_LOGI("Wi-Fi connected: ip=%s rssi=%ld mac=%s", ip().c_str(), static_cast<long>(rssi()),
              mac().c_str());
      return true;
    }
    delay(250);
    if (portalActive_) {
      dnsServer_.processNextRequest();
    }
  }

  state_ = WifiState::ConnectFailed;
  ZO_LOGW("Wi-Fi connect timeout after %lu ms", static_cast<unsigned long>(timeoutMs));
  return false;
}

bool WifiManager::reconnect(uint32_t timeoutMs) {
  if (cfg_.ssid.isEmpty()) {
    state_ = WifiState::Unconfigured;
    return false;
  }
  ZO_LOGI("Wi-Fi reconnect requested");
  WiFi.disconnect(false, false);
  const bool ok = connect(timeoutMs);
  if (!ok && !portalActive_) {
    startApPortal();
  }
  return ok;
}

void WifiManager::disconnect() {
  WiFi.disconnect(false, false);
  stopApPortal();
  state_ = WifiState::ConnectFailed;
}

bool WifiManager::applyConfigAndConnect(const WifiConfig& cfg, uint32_t timeoutMs) {
  cfg_ = cfg;
  if (cfg_.ssid.isEmpty()) {
    ZO_LOGW("Cannot apply Wi-Fi config: empty SSID");
    state_ = WifiState::Unconfigured;
    startApPortal();
    return false;
  }

  ZO_LOGI("Applying new Wi-Fi configuration for SSID '%s'", cfg_.ssid.c_str());
  const bool ok = reconnect(timeoutMs);
  if (ok) {
    ZO_LOGI("New Wi-Fi configuration connected successfully");
  } else {
    ZO_LOGW("New Wi-Fi configuration failed, AP portal remains active");
    startApPortal();
  }
  return ok;
}

WifiState WifiManager::state() const {
  return state_;
}

String WifiManager::statusText() const {
  switch (state_) {
    case WifiState::Connected:
      return "connected";
    case WifiState::Connecting:
      return "connecting";
    case WifiState::Unconfigured:
      return "unconfigured";
    case WifiState::ApMode:
      return "ap-mode";
    case WifiState::PortalActive:
      return "portal-active";
    case WifiState::ConnectFailed:
    default:
      return "connect-failed";
  }
}

String WifiManager::ssid() const {
  return cfg_.ssid;
}

String WifiManager::ip() const {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  if (portalActive_) {
    return apIp_.toString();
  }
  return "-";
}

String WifiManager::mac() const {
  return WiFi.macAddress();
}

int32_t WifiManager::rssi() const {
  if (WiFi.status() != WL_CONNECTED) {
    return 0;
  }
  return WiFi.RSSI();
}

uint32_t WifiManager::apRetries() const {
  return apRetries_;
}

bool WifiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool WifiManager::isPortalActive() const {
  return portalActive_;
}

String WifiManager::apSsid() const {
  return apSsid_;
}

String WifiManager::apPassword() const {
  return apPassword_;
}

String WifiManager::portalIp() const {
  return apIp_.toString();
}

bool WifiManager::startApPortal() {
  if (portalActive_) {
    state_ = WifiState::PortalActive;
    return true;
  }

  apSsid_ = buildApSsid();

  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAPConfig(apIp_, apGateway_, apSubnet_)) {
    state_ = WifiState::ConnectFailed;
    ZO_LOGE("Failed to configure AP IP settings");
    return false;
  }

  const bool apStarted = WiFi.softAP(apSsid_.c_str(), apPassword_.c_str());
  if (!apStarted) {
    state_ = WifiState::ConnectFailed;
    ZO_LOGE("Failed to start AP '%s'", apSsid_.c_str());
    return false;
  }

  state_ = WifiState::ApMode;
  if (!dnsServer_.start(kDnsPort, "*", apIp_)) {
    ZO_LOGE("Failed to start captive DNS server");
    portalActive_ = false;
    return false;
  }

  dnsServer_.setErrorReplyCode(DNSReplyCode::NoError);
  portalActive_ = true;
  state_ = WifiState::PortalActive;
  ZO_LOGI("Captive AP started: ssid='%s' ip=%s", apSsid_.c_str(), apIp_.toString().c_str());
  return true;
}

void WifiManager::stopApPortal() {
  if (!portalActive_) {
    return;
  }
  dnsServer_.stop();
  WiFi.softAPdisconnect(true);
  portalActive_ = false;
  ZO_LOGI("Captive AP stopped");
}

String WifiManager::buildApSsid() const {
  String base = deviceName_;
  if (base.isEmpty()) {
    base = "ZivyObraz";
  }

  String chipHex = String(static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFF), HEX);
  chipHex.toUpperCase();
  while (chipHex.length() < 4) {
    chipHex = "0" + chipHex;
  }

  return base + "-" + chipHex;
}

}  // namespace zivyobraz::net
