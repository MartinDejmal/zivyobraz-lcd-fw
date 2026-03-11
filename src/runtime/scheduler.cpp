#include "scheduler.h"

#include "diagnostics/log.h"

namespace zivyobraz::runtime {

namespace {
constexpr uint32_t kTickIntervalMs = 5000;
}

void Scheduler::begin(net::WifiManager* wifi, protocol::ProtocolCompatService* protocol) {
  wifi_ = wifi;
  protocol_ = protocol;
  state_ = SchedulerState::Idle;
  lastTickMs_ = millis();
}

void Scheduler::tick() {
  if (wifi_ == nullptr || protocol_ == nullptr) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastTickMs_ < kTickIntervalMs) {
    return;
  }
  lastTickMs_ = now;

  state_ = SchedulerState::Tick;
  wifi_->tick();
  ZO_LOGI("Scheduler tick, wifi=%s", wifi_->statusText().c_str());

  state_ = SchedulerState::Sync;
  auto rsp = protocol_->performSync(true, "TODO_API_KEY");
  if (protocol_->shouldFetchImage(lastTimestamp_, rsp.headers.timestamp)) {
    ZO_LOGI("Timestamp changed or empty -> image fetch path ready (stub)");
  }
  lastTimestamp_ = rsp.headers.timestamp;

  state_ = SchedulerState::Idle;
}

}  // namespace zivyobraz::runtime
