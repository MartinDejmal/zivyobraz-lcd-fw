#pragma once

#include <Arduino.h>

#include "project_config.h"

namespace zivyobraz::net {

enum class WifiState : uint8_t {
  Disconnected,
  Connecting,
  Connected,
  ApFallback,
};

class WifiManager {
 public:
  void begin(const WifiConfig& cfg);
  void tick();
  bool connect();
  void disconnect();
  WifiState state() const;
  String statusText() const;

 private:
  WifiConfig cfg_{};
  WifiState state_{WifiState::Disconnected};
};

}  // namespace zivyobraz::net
