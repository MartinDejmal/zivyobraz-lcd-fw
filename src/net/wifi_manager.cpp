#include "wifi_manager.h"

#include <WiFi.h>

#include "diagnostics/log.h"

namespace zivyobraz::net {

namespace {
constexpr uint32_t kReconnectIntervalMs = 10000;
}

void WifiManager::begin(const WifiConfig& cfg) {
  cfg_ = cfg;
  apRetries_ = 0;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  if (cfg_.ssid.isEmpty()) {
    state_ = WifiState::NotConfigured;
    ZO_LOGW("Wi-Fi not configured (empty SSID)");
    return;
  }

  state_ = WifiState::Disconnected;
  ZO_LOGI("WifiManager initialized, ssid='%s'", cfg_.ssid.c_str());
}

void WifiManager::tick() {
  if (state_ == WifiState::NotConfigured) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    state_ = WifiState::Connected;
    return;
  }

  state_ = WifiState::Disconnected;
  const uint32_t now = millis();
  if (now - lastReconnectAttemptMs_ >= kReconnectIntervalMs) {
    lastReconnectAttemptMs_ = now;
    reconnect();
  }
}

bool WifiManager::connect(uint32_t timeoutMs) {
  if (cfg_.ssid.isEmpty()) {
    state_ = WifiState::NotConfigured;
    return false;
  }

  if (WiFi.status() == WL_CONNECTED) {
    state_ = WifiState::Connected;
    return true;
  }

  state_ = WifiState::Connecting;
  ++apRetries_;
  ZO_LOGI("Wi-Fi connecting attempt #%lu to SSID '%s'", static_cast<unsigned long>(apRetries_),
          cfg_.ssid.c_str());

  WiFi.begin(cfg_.ssid.c_str(), cfg_.password.c_str());

  const uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      state_ = WifiState::Connected;
      ZO_LOGI("Wi-Fi connected: ip=%s rssi=%ld mac=%s", ip().c_str(), static_cast<long>(rssi()),
              mac().c_str());
      return true;
    }
    delay(250);
  }

  state_ = WifiState::Disconnected;
  ZO_LOGW("Wi-Fi connect timeout after %lu ms", static_cast<unsigned long>(timeoutMs));
  return false;
}

bool WifiManager::reconnect(uint32_t timeoutMs) {
  if (state_ == WifiState::NotConfigured) {
    return false;
  }
  ZO_LOGI("Wi-Fi reconnect requested");
  WiFi.disconnect(false, false);
  return connect(timeoutMs);
}

void WifiManager::disconnect() {
  WiFi.disconnect(false, false);
  state_ = WifiState::Disconnected;
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
    case WifiState::NotConfigured:
      return "not configured";
    case WifiState::ApFallback:
      return "ap-fallback";
    case WifiState::Disconnected:
    default:
      return "disconnected";
  }
}

String WifiManager::ssid() const {
  return cfg_.ssid;
}

String WifiManager::ip() const {
  if (WiFi.status() != WL_CONNECTED) {
    return "-";
  }
  return WiFi.localIP().toString();
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

}  // namespace zivyobraz::net
