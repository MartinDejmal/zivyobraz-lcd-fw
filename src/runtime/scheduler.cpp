#include "scheduler.h"

#include "diagnostics/log.h"

namespace zivyobraz::runtime {

namespace {
constexpr uint32_t kTickIntervalMs = 30000;
}

void Scheduler::begin(config::ConfigManager* config, net::WifiManager* wifi,
                      protocol::ProtocolCompatService* protocol) {
  config_ = config;
  wifi_ = wifi;
  protocol_ = protocol;
  state_ = SchedulerState::Idle;
  lastTickMs_ = 0;

  if (wifi_ != nullptr) {
    wifi_->connect(12000);
  }
}

void Scheduler::tick() {
  if (wifi_ == nullptr || protocol_ == nullptr || config_ == nullptr) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastTickMs_ < kTickIntervalMs) {
    wifi_->tick();
    return;
  }
  lastTickMs_ = now;

  state_ = SchedulerState::Tick;
  wifi_->tick();
  ZO_LOGI("Scheduler tick, wifi=%s ip=%s rssi=%ld retries=%lu", wifi_->statusText().c_str(),
          wifi_->ip().c_str(), static_cast<long>(wifi_->rssi()),
          static_cast<unsigned long>(wifi_->apRetries()));

  if (!wifi_->isConnected()) {
    ZO_LOGW("Scheduler: skipping protocol sync, Wi-Fi not connected");
    state_ = SchedulerState::Idle;
    return;
  }

  state_ = SchedulerState::Sync;
  lastProtocolResponse_ = protocol_->performSync(true, config_->apiKey(), config_->lastTimestamp());

  if (!lastProtocolResponse_.candidateTimestamp.isEmpty()) {
    if (lastProtocolResponse_.hasNewContent) {
      ZO_LOGI("Timestamp changed: previous=%s candidate=%s", config_->lastTimestamp().c_str(),
              lastProtocolResponse_.candidateTimestamp.c_str());
      // Intentionally not persisted yet. The timestamp should be committed only after
      // successful body decode and render in the next milestone.
    } else {
      ZO_LOGI("Timestamp unchanged: %s", lastProtocolResponse_.candidateTimestamp.c_str());
    }
  }

  state_ = SchedulerState::Idle;
}

String Scheduler::wifiDiagnostics() const {
  if (wifi_ == nullptr) {
    return "WiFi: n/a";
  }
  String s;
  s.reserve(160);
  s += "WiFi: ";
  s += wifi_->statusText();
  s += "\nSSID: ";
  s += wifi_->ssid();
  s += "\nIP: ";
  s += wifi_->ip();
  s += "\nRSSI: ";
  s += String(wifi_->rssi());
  s += " dBm";
  s += "\nRetries: ";
  s += String(wifi_->apRetries());
  return s;
}

String Scheduler::protocolDiagnostics() const {
  if (config_ == nullptr) {
    return "Protocol: n/a";
  }

  String s;
  s.reserve(280);
  s += "API: ";
  s += config_->apiKey();
  s += "\nHost: ";
  s += config_->get().server.host;
  s += "\nHTTP: ";
  if (lastProtocolResponse_.status == protocol::TransportStatus::NotAttempted) {
    s += "not attempted";
    return s;
  }

  s += lastProtocolResponse_.httpOk ? "200 OK" : String(lastProtocolResponse_.httpStatusCode);
  s += "\nTS: ";
  s += lastProtocolResponse_.headers.timestamp.isEmpty() ? "-" : lastProtocolResponse_.headers.timestamp;
  s += "\nChanged: ";
  s += lastProtocolResponse_.hasNewContent ? "yes" : "no";
  s += "\nPreciseSleep: ";
  s += String(lastProtocolResponse_.headers.preciseSleepSec);
  s += "\nOTA: ";
  s += lastProtocolResponse_.hasOtaUpdate ? "yes" : "no";
  return s;
}

}  // namespace zivyobraz::runtime
