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
  ResponseHeaders headers{};
  size_t bodySize{0};
};

}  // namespace zivyobraz::protocol
