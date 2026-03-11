#pragma once

#include <Arduino.h>

#include "config/config_manager.h"
#include "net/wifi_manager.h"
#include "protocol/protocol_compat.h"

namespace zivyobraz::runtime {

enum class SchedulerState : uint8_t {
  Idle,
  Tick,
  Sync,
};

class Scheduler {
 public:
  void begin(config::ConfigManager* config, net::WifiManager* wifi,
             protocol::ProtocolCompatService* protocol);
  void tick();

  String wifiDiagnostics() const;
  String protocolDiagnostics() const;

 private:
  config::ConfigManager* config_{nullptr};
  net::WifiManager* wifi_{nullptr};
  protocol::ProtocolCompatService* protocol_{nullptr};
  SchedulerState state_{SchedulerState::Idle};
  uint32_t lastTickMs_{0};
  protocol::ProtocolResponse lastProtocolResponse_{};
};

}  // namespace zivyobraz::runtime
