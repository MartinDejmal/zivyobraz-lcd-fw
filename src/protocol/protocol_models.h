#pragma once

#include <Arduino.h>

namespace zivyobraz::protocol {

struct RequestMetadata {
  String fwVersion;
  String apiVersion;
  String buildDate;
  String board;
  String system;
  String network;
  String display;
  String sensors;
  String wifiSsid;
  String localIp;
  String mac;
  int32_t rssi{0};
};

struct ResponseHeaders {
  String timestamp;
  uint32_t preciseSleepSec{0};
  int8_t rotate{-1};
  bool partialRefresh{false};
  bool showNoWifiError{true};
  String otaUpdateUrl;
};

enum class TransportStatus : uint8_t {
  NotAttempted,
  Ok,
  NetworkError,
  HttpError,
  ParseError,
};

struct ProtocolResponse {
  TransportStatus status{TransportStatus::NotAttempted};
  int httpStatusCode{0};
  bool transportOk{false};
  bool httpOk{false};
  bool bodyPresent{false};
  size_t bodySize{0};
  bool hasNewContent{false};
  String previousTimestamp;
  String candidateTimestamp;
  bool hasOtaUpdate{false};
  ResponseHeaders headers{};
  String errorMessage;
};

}  // namespace zivyobraz::protocol
