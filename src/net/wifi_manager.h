#pragma once

#include <Arduino.h>

#include "project_config.h"

namespace zivyobraz::net {

enum class WifiState : uint8_t {
  Disconnected,
  Connecting,
  Connected,
  NotConfigured,
  ApFallback,
};

class WifiManager {
 public:
  void begin(const WifiConfig& cfg);
  void tick();
  bool connect(uint32_t timeoutMs = 15000);
  bool reconnect(uint32_t timeoutMs = 8000);
  void disconnect();

  WifiState state() const;
  String statusText() const;
  String ssid() const;
  String ip() const;
  String mac() const;
  int32_t rssi() const;
  uint32_t apRetries() const;
  bool isConnected() const;

 private:
  WifiConfig cfg_{};
  WifiState state_{WifiState::Disconnected};
  uint32_t apRetries_{0};
  uint32_t lastReconnectAttemptMs_{0};
};

}  // namespace zivyobraz::net
