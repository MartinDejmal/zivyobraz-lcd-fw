#include "protocol_compat.h"

#include "diagnostics/log.h"
#include "project_config.h"

namespace zivyobraz::protocol {

void ProtocolCompatService::begin(const RuntimeConfig& cfg) {
  cfg_ = cfg;
}

RequestMetadata ProtocolCompatService::buildMetadataPayload() const {
  RequestMetadata payload;
  payload.fwVersion = cfg_.versions.fwVersion;
  payload.apiVersion = cfg_.versions.apiVersion;
  payload.buildDate = ZO_BUILD_DATE;
  payload.board = cfg_.boardName;
  payload.system = "esp32-arduino";
  payload.network = "wifi";
  payload.display = "st7789-240x240";
  payload.sensors = "";
  return payload;
}

bool ProtocolCompatService::shouldFetchImage(const String& previousTimestamp,
                                             const String& incomingTimestamp) const {
  if (incomingTimestamp.isEmpty()) {
    return true;
  }
  return previousTimestamp != incomingTimestamp;
}

ProtocolResponse ProtocolCompatService::performSync(bool timestampCheck, const String& apiKey) {
  (void)apiKey;
  // TODO: Implement HTTPS POST /index.php?timestampCheck={0|1} with X-API-Key header.
  // TODO: Parse response headers Timestamp/PreciseSleep/Rotate/PartialRefresh/ShowNoWifiError/X-OTA-Update.
  // TODO: Route body stream to image decoder facade.
  ProtocolResponse rsp;
  rsp.status = TransportStatus::NotAttempted;
  rsp.headers.timestamp = timestampCheck ? "stub-ts-check" : "stub-no-check";
  ZO_LOGI("Protocol sync stub called (timestampCheck=%d)", timestampCheck ? 1 : 0);
  return rsp;
}

}  // namespace zivyobraz::protocol
