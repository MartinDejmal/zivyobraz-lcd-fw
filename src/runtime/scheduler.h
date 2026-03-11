#pragma once

#include <Arduino.h>

#include "config/config_manager.h"
#include "display/display_hal.h"
#include "image/image_decoder.h"
#include "image/indexed_framebuffer.h"
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
             protocol::ProtocolCompatService* protocol, display::IDisplay* display);
  void tick();

  String wifiDiagnostics() const;
  String protocolDiagnostics() const;
  display::StatusSnapshot statusSnapshot() const;

 private:
  String formatName(image::ImageFormat fmt) const;
  String resultClassName(protocol::ProtocolResultClass rc) const;
  String bodyProbeName(protocol::BodyProbeKind kind) const;

  config::ConfigManager* config_{nullptr};
  net::WifiManager* wifi_{nullptr};
  protocol::ProtocolCompatService* protocol_{nullptr};
  display::IDisplay* display_{nullptr};
  SchedulerState state_{SchedulerState::Idle};
  uint32_t lastTickMs_{0};
  protocol::ProtocolResponse lastProtocolResponse_{};
  String lastRenderResult_{"n/a"};
  bool pendingDiagnosticImageFetch_{false};
};

}  // namespace zivyobraz::runtime
