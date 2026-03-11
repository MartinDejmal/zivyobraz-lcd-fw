#pragma once

#include <Arduino.h>

#include "image/image_decoder.h"

namespace zivyobraz::protocol {

struct RequestMetadata {
  String fwVersion;
  String apiVersion;
  String buildDate;
  String board;
  String systemResetReason;
  String wifiSsid;
  String ipAddress;
  String mac;
  int32_t rssi{0};
  uint32_t apRetries{0};
  String displayType;
  uint16_t displayWidth{0};
  uint16_t displayHeight{0};
  String displayColorType;
};

struct ResponseHeaders {
  String timestamp;
  uint32_t preciseSleepSec{0};
  int8_t rotate{-1};
  bool partialRefresh{false};
  bool showNoWifiError{true};
  String otaUpdateUrl;
};

enum class BodyProbeKind : uint8_t {
  None,
  Png,
  Z1,
  Z2,
  Z3,
  Html,
  Text,
  Unknown,
};

enum class ProtocolResultClass : uint8_t {
  None,
  SuccessChanged,
  SuccessUnchanged,
  NetworkFailure,
  HttpFailure,
  ParseFailure,
  ProtocolMissingTimestamp,
  ProtocolBodyMissing,
  ProtocolDecodeRenderFailure,
  ProtocolRequestInvalid,
};

enum class TransportStatus : uint8_t {
  NotAttempted,
  Ok,
  NetworkError,
  HttpError,
  ParseError,
  ProtocolError,
};

struct ProtocolResponse {
  TransportStatus status{TransportStatus::NotAttempted};
  int httpStatusCode{0};
  bool transportOk{false};
  bool httpOk{false};
  bool bodyPresent{false};
  size_t bodySize{0};
  bool hasNewContent{false};
  bool bodyHandled{false};
  bool renderSuccess{false};
  String previousTimestamp;
  String candidateTimestamp;
  bool hasOtaUpdate{false};
  ResponseHeaders headers{};
  image::DecodeResult decode{};
  String renderMessage;
  String errorMessage;
  ProtocolResultClass resultClass{ProtocolResultClass::None};
  bool missingRequiredTimestamp{false};
  BodyProbeKind bodyProbeKind{BodyProbeKind::None};
  int32_t bodyProbeOffset{-1};
  String bodyPreviewHex;
  String bodyPreviewAscii;
};

}  // namespace zivyobraz::protocol
