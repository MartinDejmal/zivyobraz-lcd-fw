#include "scheduler.h"

#include "diagnostics/log.h"

namespace zivyobraz::runtime {

namespace {
constexpr uint32_t kTickIntervalMs = 90000;
constexpr size_t kProbeLimit = 4096;
}

void Scheduler::begin(config::ConfigManager* config, net::WifiManager* wifi,
                      protocol::ProtocolCompatService* protocol, display::IDisplay* display) {
  config_ = config;
  wifi_ = wifi;
  protocol_ = protocol;
  display_ = display;
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
  image::ImageDecoderFacade decoder;
  image::IndexedFramebuffer framebuffer;
  framebuffer.resize(config_->get().display.width, config_->get().display.height);

  bool timestampCheck = true;
  if (pendingDiagnosticImageFetch_ &&
      config_->get().protocolDebug.forceTimestampCheckZeroOnMissingTimestamp) {
    timestampCheck = false;
    pendingDiagnosticImageFetch_ = false;
    ZO_LOGW("Protocol diagnostic fallback: forcing one cycle timestampCheck=0");
  }

  lastProtocolResponse_ = protocol_->performSync(
      timestampCheck, config_->apiKey(), config_->lastTimestamp(), wifi_->apRetries(),
      [&](image::IByteStream& stream, protocol::ProtocolResponse& rsp) {
        rsp.decode = decoder.decode(stream, framebuffer, kProbeLimit);
        ZO_LOGI("Decode selected format=%d result=%d pixels=%u", static_cast<int>(rsp.decode.format),
                static_cast<int>(rsp.decode.status), static_cast<unsigned int>(rsp.decode.pixelsDecoded));

        if (!rsp.decode.success) {
          return false;
        }

        if (display_ == nullptr) {
          rsp.renderMessage = "Display unavailable";
          return false;
        }

        const bool renderOk =
            display_->renderIndexed(framebuffer, rsp.headers.rotate, rsp.headers.partialRefresh);
        rsp.renderSuccess = renderOk;
        rsp.renderMessage = renderOk ? "Render OK" : "Render failed";
        return renderOk;
      });

  if (config_->get().protocolDebug.forceTimestampCheckZeroOnMissingTimestamp &&
      lastProtocolResponse_.missingRequiredTimestamp && timestampCheck) {
    pendingDiagnosticImageFetch_ = true;
  }

  if (lastProtocolResponse_.resultClass == protocol::ProtocolResultClass::SuccessChanged) {
    if (!lastProtocolResponse_.bodyPresent) {
      ZO_LOGW("Timestamp not committed: body missing");
      lastRenderResult_ = "No body";
      lastProtocolResponse_.resultClass = protocol::ProtocolResultClass::ProtocolBodyMissing;
    } else if (!lastProtocolResponse_.decode.success || !lastProtocolResponse_.renderSuccess) {
      ZO_LOGW("Timestamp not committed: decode/render failed");
      lastRenderResult_ = lastProtocolResponse_.renderMessage;
      if (lastRenderResult_.isEmpty()) {
        lastRenderResult_ = lastProtocolResponse_.decode.errorMessage;
      }
      lastProtocolResponse_.resultClass = protocol::ProtocolResultClass::ProtocolDecodeRenderFailure;
    } else {
      config_->setLastTimestamp(lastProtocolResponse_.candidateTimestamp);
      config_->save();
      ZO_LOGI("Timestamp committed: %s", lastProtocolResponse_.candidateTimestamp.c_str());
      lastRenderResult_ = "Render OK + TS commit";
    }
  } else if (lastProtocolResponse_.resultClass == protocol::ProtocolResultClass::SuccessUnchanged) {
    if (lastProtocolResponse_.bodyPresent) {
      if (!lastProtocolResponse_.decode.success || !lastProtocolResponse_.renderSuccess) {
        ZO_LOGW("Unchanged response body decode/render failed");
        lastRenderResult_ = lastProtocolResponse_.renderMessage;
        if (lastRenderResult_.isEmpty()) {
          lastRenderResult_ = lastProtocolResponse_.decode.errorMessage;
        }
        lastProtocolResponse_.resultClass =
            protocol::ProtocolResultClass::ProtocolDecodeRenderFailure;
      } else {
        ZO_LOGI("Timestamp unchanged, body rendered: %s",
                lastProtocolResponse_.candidateTimestamp.c_str());
        lastRenderResult_ = "Render OK (unchanged)";
      }
    } else {
      ZO_LOGI("Timestamp unchanged: %s", lastProtocolResponse_.candidateTimestamp.c_str());
      lastRenderResult_ = "Skipped (unchanged)";
    }
  } else if (lastProtocolResponse_.missingRequiredTimestamp) {
    lastRenderResult_ = "Missing Timestamp";
  }

  state_ = SchedulerState::Idle;
}

bool Scheduler::hasValidImage() const {
  if (lastProtocolResponse_.resultClass == protocol::ProtocolResultClass::SuccessChanged) {
    return lastProtocolResponse_.renderSuccess;
  }

  if (lastProtocolResponse_.resultClass == protocol::ProtocolResultClass::SuccessUnchanged) {
    return !lastProtocolResponse_.bodyPresent ||
           (lastProtocolResponse_.decode.success && lastProtocolResponse_.renderSuccess);
  }

  return false;
}

String Scheduler::wifiDiagnostics() const {
  if (wifi_ == nullptr) {
    return "n/a";
  }
  String s;
  s.reserve(64);
  s += wifi_->statusText();
  s += " ";
  s += wifi_->ip();
  return s;
}

String Scheduler::protocolDiagnostics() const {
  if (config_ == nullptr) {
    return "n/a";
  }

  if (lastProtocolResponse_.status == protocol::TransportStatus::NotAttempted) {
    return "not attempted";
  }

  String s = String(lastProtocolResponse_.httpStatusCode) + " " + resultClassName(lastProtocolResponse_.resultClass);
  if (!lastProtocolResponse_.errorMessage.isEmpty()) {
    s += " (" + lastProtocolResponse_.errorMessage + ")";
  }
  return s;
}

display::StatusSnapshot Scheduler::statusSnapshot() const {
  display::StatusSnapshot s;
  if (config_ == nullptr) {
    return s;
  }

  s.fwVersion = config_->get().versions.fwVersion;
  s.wifiStatus = wifiDiagnostics();
  s.apiKey = config_->apiKey();
  s.serverHost = config_->get().server.host;
  s.protocolStatus = protocolDiagnostics();
  s.protocolDebug = config_->get().protocolDebug.wireDebug ? "on" : "off";
  s.timestampMissing = lastProtocolResponse_.missingRequiredTimestamp ? "yes" : "no";
  s.detectedFormat = formatName(lastProtocolResponse_.decode.format);
  s.signatureOffset = String(lastProtocolResponse_.decode.signatureOffset);
  s.decodeResult = lastProtocolResponse_.decode.success ? "OK" : lastProtocolResponse_.decode.errorMessage;
  s.pixelsDecoded = String(lastProtocolResponse_.decode.pixelsDecoded);
  s.storedTimestamp = config_->lastTimestamp();
  s.candidateTimestamp = lastProtocolResponse_.candidateTimestamp;
  s.renderResult = lastRenderResult_;
  s.bodyProbe = bodyProbeName(lastProtocolResponse_.bodyProbeKind);
  s.bodyProbeOffset = String(lastProtocolResponse_.bodyProbeOffset);
  return s;
}

String Scheduler::formatName(image::ImageFormat fmt) const {
  switch (fmt) {
    case image::ImageFormat::Png:
      return "PNG";
    case image::ImageFormat::Z1:
      return "Z1";
    case image::ImageFormat::Z2:
      return "Z2";
    case image::ImageFormat::Z3:
      return "Z3";
    default:
      return "unknown";
  }
}

String Scheduler::resultClassName(protocol::ProtocolResultClass rc) const {
  switch (rc) {
    case protocol::ProtocolResultClass::SuccessChanged:
      return "changed";
    case protocol::ProtocolResultClass::SuccessUnchanged:
      return "unchanged";
    case protocol::ProtocolResultClass::NetworkFailure:
      return "net_err";
    case protocol::ProtocolResultClass::HttpFailure:
      return "http_err";
    case protocol::ProtocolResultClass::ParseFailure:
      return "parse_err";
    case protocol::ProtocolResultClass::ProtocolMissingTimestamp:
      return "missing_ts";
    case protocol::ProtocolResultClass::ProtocolBodyMissing:
      return "missing_body";
    case protocol::ProtocolResultClass::ProtocolDecodeRenderFailure:
      return "decode_render";
    case protocol::ProtocolResultClass::ProtocolRequestInvalid:
      return "request_invalid";
    default:
      return "unknown";
  }
}

String Scheduler::bodyProbeName(protocol::BodyProbeKind kind) const {
  switch (kind) {
    case protocol::BodyProbeKind::Png:
      return "PNG";
    case protocol::BodyProbeKind::Z1:
      return "Z1";
    case protocol::BodyProbeKind::Z2:
      return "Z2";
    case protocol::BodyProbeKind::Z3:
      return "Z3";
    case protocol::BodyProbeKind::Html:
      return "HTML";
    case protocol::BodyProbeKind::Text:
      return "text";
    case protocol::BodyProbeKind::Unknown:
      return "unknown";
    default:
      return "none";
  }
}

}  // namespace zivyobraz::runtime
