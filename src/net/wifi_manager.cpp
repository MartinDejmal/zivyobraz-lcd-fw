#include "wifi_manager.h"

#include "diagnostics/log.h"

namespace zivyobraz::net {

void WifiManager::begin(const WifiConfig& cfg) {
  cfg_ = cfg;
  state_ = WifiState::Disconnected;
  ZO_LOGI("WifiManager initialized (stub), ssid='%s'", cfg_.ssid.c_str());
}

void WifiManager::tick() {
  // TODO: implement STA/AP state machine.
}

bool WifiManager::connect() {
  state_ = WifiState::Connecting;
  // TODO: implement Wi-Fi connection sequence.
  state_ = WifiState::Disconnected;
  ZO_LOGW("Wi-Fi connect stub invoked");
  return false;
}

void WifiManager::disconnect() {
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
    case WifiState::ApFallback:
      return "ap-fallback";
    case WifiState::Disconnected:
    default:
      return "not connected (stub)";
  }
}

}  // namespace zivyobraz::net
