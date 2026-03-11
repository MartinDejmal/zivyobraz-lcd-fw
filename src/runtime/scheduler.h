#pragma once

#include <Arduino.h>

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
  void begin(net::WifiManager* wifi, protocol::ProtocolCompatService* protocol);
  void tick();

 private:
  net::WifiManager* wifi_{nullptr};
  protocol::ProtocolCompatService* protocol_{nullptr};
  SchedulerState state_{SchedulerState::Idle};
  uint32_t lastTickMs_{0};
  String lastTimestamp_{};
};

}  // namespace zivyobraz::runtime
