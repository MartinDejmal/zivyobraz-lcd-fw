#include "protocol_compat.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <vector>

#include <esp_system.h>

#include "diagnostics/log.h"
#include "project_config.h"
#include "protocol/http_body_stream.h"

namespace zivyobraz::protocol {

namespace {
constexpr size_t kBodyPreviewLimit = 256;

String jsonEscape(const String& input) {
  String out;
  out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    if (c == '\\' || c == '"') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else {
      out += c;
    }
  }
  return out;
}

String stripTrailingCr(const String& line) {
  if (line.length() > 0 && line[line.length() - 1] == '\r') {
    return line.substring(0, line.length() - 1);
  }
  return line;
}

String trimLeadingSpaces(const String& value) {
  size_t i = 0;
  while (i < value.length() && (value[i] == ' ' || value[i] == '\t')) {
    ++i;
  }
  return value.substring(i);
}

String formatHexPreview(const uint8_t* data, size_t len) {
  String out;
  out.reserve(len * 3 + 16);
  for (size_t i = 0; i < len; ++i) {
    if (i > 0) {
      out += ' ';
    }
    char b[4] = {0};
    snprintf(b, sizeof(b), "%02X", data[i]);
    out += b;
  }
  return out;
}

String formatAsciiPreview(const uint8_t* data, size_t len) {
  String out;
  out.reserve(len + 16);
  for (size_t i = 0; i < len; ++i) {
    const uint8_t c = data[i];
    out += (c >= 32 && c <= 126) ? static_cast<char>(c) : '.';
  }
  return out;
}

class PreviewingBodyStream final : public image::IByteStream {
 public:
  explicit PreviewingBodyStream(HttpBodyStream& inner) : inner_(inner) {
    preview_.reserve(kBodyPreviewLimit);
  }

  int readByte() override {
    const int b = inner_.readByte();
    if (b >= 0 && preview_.size() < kBodyPreviewLimit) {
      preview_.push_back(static_cast<uint8_t>(b));
    }
    return b;
  }

  const std::vector<uint8_t>& preview() const {
    return preview_;
  }

 private:
  HttpBodyStream& inner_;
  std::vector<uint8_t> preview_;
};

}  // namespace

void ProtocolCompatService::begin(const RuntimeConfig& cfg) {
  cfg_ = cfg;
}

RequestMetadata ProtocolCompatService::buildMetadataPayload() const {
  RequestMetadata payload;
  payload.fwVersion = cfg_.versions.fwVersion;
  payload.apiVersion = cfg_.versions.apiVersion;
  payload.buildDate = ZO_BUILD_DATE;
  payload.board = cfg_.boardName;
  payload.systemResetReason = String(esp_reset_reason());
  payload.wifiSsid = WiFi.SSID();
  payload.ipAddress = WiFi.localIP().toString();
  payload.mac = WiFi.macAddress();
  payload.rssi = WiFi.RSSI();
  payload.displayType = "st7789";
  payload.displayWidth = cfg_.display.width;
  payload.displayHeight = cfg_.display.height;
  payload.displayColorType = "7C";
  return payload;
}

bool ProtocolCompatService::shouldFetchImage(const String& previousTimestamp,
                                             const String& incomingTimestamp) const {
  if (incomingTimestamp.isEmpty()) {
    return true;
  }
  return previousTimestamp != incomingTimestamp;
}

ProtocolResponse ProtocolCompatService::performSync(bool timestampCheck, const String& apiKey,
                                                    const String& previousTimestamp, uint32_t apRetries,
                                                    const BodyHandler& bodyHandler) {
  ProtocolResponse rsp;
  rsp.previousTimestamp = previousTimestamp;

  if (WiFi.status() != WL_CONNECTED) {
    rsp.status = TransportStatus::NetworkError;
    rsp.resultClass = ProtocolResultClass::NetworkFailure;
    rsp.errorMessage = "Wi-Fi disconnected";
    ZO_LOGW("Protocol sync skipped: Wi-Fi disconnected");
    return rsp;
  }

  RequestMetadata metadata = buildMetadataPayload();
  metadata.apRetries = apRetries;

  String validationError;
  if (!validateRequestMetadata(metadata, apiKey, validationError)) {
    rsp.status = TransportStatus::ProtocolError;
    rsp.resultClass = ProtocolResultClass::ProtocolRequestInvalid;
    rsp.errorMessage = validationError;
    ZO_LOGE("Request validation failed: %s", validationError.c_str());
    return rsp;
  }

  const String body = buildMetadataJson(metadata);
  const String path = cfg_.server.endpointPath + "?timestampCheck=" + String(timestampCheck ? 1 : 0);
  const uint16_t port = cfg_.server.useHttps ? 443 : 80;

  if (wireDebugEnabled()) {
    std::vector<String> systemFields;
    if (!metadata.systemResetReason.isEmpty()) {
      systemFields.push_back("resetReason");
    }
    std::vector<String> networkFields{"ssid", "rssi", "mac", "apRetries", "ipAddress"};
    std::vector<String> displayFields{"type", "width", "height", "colorType"};

    ZO_LOGI("[WireDbg] Request schema boardKind=string systemFields=%s networkFields=%s displayFields=%s apiKeyValid=%s",
            joinFieldList(systemFields).c_str(), joinFieldList(networkFields).c_str(),
            joinFieldList(displayFields).c_str(), (apiKey.length() == 8) ? "yes" : "no");
    ZO_LOGI("[WireDbg] Request transport=%s host=%s path=%s contentLength=%u apiKey=%s timestampCheck=%d",
            cfg_.server.useHttps ? "https" : "http", cfg_.server.host.c_str(), path.c_str(),
            static_cast<unsigned int>(body.length()), apiKey.c_str(), timestampCheck ? 1 : 0);
    ZO_LOGI("[WireDbg] Request payload=%s", body.c_str());
  }

  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  Client* client = nullptr;
  if (cfg_.server.useHttps) {
    secureClient.setInsecure();
    client = &secureClient;
  } else {
    client = &plainClient;
  }

  ZO_LOGI("HTTP begin: %s://%s:%u%s", cfg_.server.useHttps ? "https" : "http",
          cfg_.server.host.c_str(), port, path.c_str());
  ZO_LOGI("Payload size: %u", static_cast<unsigned int>(body.length()));

  if (!client->connect(cfg_.server.host.c_str(), port)) {
    rsp.status = TransportStatus::NetworkError;
    rsp.resultClass = ProtocolResultClass::NetworkFailure;
    rsp.errorMessage = "TCP connect failed";
    ZO_LOGE("TCP connect failed to %s:%u", cfg_.server.host.c_str(), port);
    return rsp;
  }

  client->setTimeout(12000);
  client->print(String("POST ") + path + " HTTP/1.1\r\n");
  client->print(String("Host: ") + cfg_.server.host + "\r\n");
  client->print(String("X-API-Key: ") + apiKey + "\r\n");
  client->print("Content-Type: application/json\r\n");
  client->print("Connection: close\r\n");
  client->print(String("Content-Length: ") + body.length() + "\r\n\r\n");
  client->print(body);

  const String rawStatusLine = client->readStringUntil('\n');
  const String statusLine = stripTrailingCr(rawStatusLine);
  if (wireDebugEnabled()) {
    ZO_LOGI("[WireDbg] Raw status line: %s", statusLine.c_str());
  }

  if (!(statusLine.startsWith("HTTP/1.1") || statusLine.startsWith("HTTP/1.0"))) {
    rsp.status = TransportStatus::ParseError;
    rsp.resultClass = ProtocolResultClass::ParseFailure;
    rsp.errorMessage = "Invalid HTTP status line";
    ZO_LOGE("Invalid HTTP response status line: %s", statusLine.c_str());
    client->stop();
    return rsp;
  }

  const int firstSpace = statusLine.indexOf(' ');
  const int secondSpace = statusLine.indexOf(' ', firstSpace + 1);
  if (firstSpace < 0 || secondSpace <= firstSpace + 1) {
    rsp.status = TransportStatus::ParseError;
    rsp.resultClass = ProtocolResultClass::ParseFailure;
    rsp.errorMessage = "Missing HTTP status code";
    client->stop();
    return rsp;
  }

  rsp.httpStatusCode = statusLine.substring(firstSpace + 1, secondSpace).toInt();
  rsp.transportOk = true;
  rsp.httpOk = (rsp.httpStatusCode == 200);

  ZO_LOGI("HTTP status: %d", rsp.httpStatusCode);
  if (!rsp.httpOk) {
    rsp.status = TransportStatus::HttpError;
    rsp.resultClass = ProtocolResultClass::HttpFailure;
    rsp.errorMessage = "Non-200 HTTP status";
  } else {
    rsp.status = TransportStatus::Ok;
  }

  uint32_t rawHeaderCount = 0;
  bool headerSeparatorFound = false;

  // Header parser assumptions:
  // - Status line already consumed and validated above.
  // - Headers are ASCII lines terminated by LF, optional trailing CR (HTTP/1.0 and HTTP/1.1).
  // - Parsing stops exactly at the first empty line separator between headers and body.
  while (client->connected() || client->available()) {
    const String rawHeaderLine = client->readStringUntil('\n');
    const String headerLine = stripTrailingCr(rawHeaderLine);
    if (headerLine.length() == 0) {
      headerSeparatorFound = true;
      break;
    }

    rawHeaderCount++;
    if (wireDebugEnabled()) {
      ZO_LOGI("[WireDbg] Raw header[%u]: %s", static_cast<unsigned int>(rawHeaderCount),
              headerLine.c_str());
    }

    const int sep = headerLine.indexOf(':');
    if (sep <= 0) {
      continue;
    }

    const String key = headerLine.substring(0, sep);
    const String value = trimLeadingSpaces(headerLine.substring(sep + 1));

    if (key == "Timestamp") {
      rsp.headers.timestamp = value;
      if (wireDebugEnabled()) {
        ZO_LOGI("[WireDbg] Parsed Timestamp='%s'", value.c_str());
      }
    } else if (key == "PreciseSleep") {
      rsp.headers.preciseSleepSec = static_cast<uint32_t>(value.toInt());
      if (wireDebugEnabled()) {
        ZO_LOGI("[WireDbg] Parsed PreciseSleep=%lu",
                static_cast<unsigned long>(rsp.headers.preciseSleepSec));
      }
    } else if (key == "Rotate") {
      rsp.headers.rotate = static_cast<int8_t>(value.toInt());
      if (wireDebugEnabled()) {
        ZO_LOGI("[WireDbg] Parsed Rotate=%d", rsp.headers.rotate);
      }
    } else if (key == "PartialRefresh") {
      rsp.headers.partialRefresh = parseBoolHeaderValue(value);
      if (wireDebugEnabled()) {
        ZO_LOGI("[WireDbg] Parsed PartialRefresh=%d", rsp.headers.partialRefresh ? 1 : 0);
      }
    } else if (key == "ShowNoWifiError") {
      rsp.headers.showNoWifiError = parseBoolHeaderValue(value);
      if (wireDebugEnabled()) {
        ZO_LOGI("[WireDbg] Parsed ShowNoWifiError=%d", rsp.headers.showNoWifiError ? 1 : 0);
      }
    } else if (key == "X-OTA-Update") {
      rsp.headers.otaUpdateUrl = value;
      rsp.hasOtaUpdate = !value.isEmpty();
      if (wireDebugEnabled()) {
        ZO_LOGI("[WireDbg] Parsed X-OTA-Update='%s'", value.c_str());
      }
    }
  }

  if (wireDebugEnabled()) {
    ZO_LOGI("[WireDbg] Header count=%u separatorFound=%d", static_cast<unsigned int>(rawHeaderCount),
            headerSeparatorFound ? 1 : 0);
  }

  rsp.candidateTimestamp = rsp.headers.timestamp;
  if (!rsp.headers.timestamp.isEmpty()) {
    rsp.hasNewContent = shouldFetchImage(previousTimestamp, rsp.headers.timestamp);
  }

  ZO_LOGI("Headers: ts='%s' changed=%d sleep=%lu rotate=%d partial=%d showNoWifi=%d ota=%d",
          rsp.headers.timestamp.c_str(), rsp.hasNewContent ? 1 : 0,
          static_cast<unsigned long>(rsp.headers.preciseSleepSec), rsp.headers.rotate,
          rsp.headers.partialRefresh ? 1 : 0, rsp.headers.showNoWifiError ? 1 : 0,
          rsp.hasOtaUpdate ? 1 : 0);

  HttpBodyStream bodyStream(*client);
  PreviewingBodyStream previewStream(bodyStream);

  if (rsp.httpOk && rsp.hasNewContent && bodyHandler) {
    ZO_LOGI("HTTP body handoff to decoder pipeline");
    rsp.bodyHandled = bodyHandler(previewStream, rsp);
  }

  while (client->available() || client->connected()) {
    const int b = previewStream.readByte();
    if (b < 0) {
      break;
    }
  }

  rsp.bodySize = bodyStream.bytesRead();
  rsp.bodyPresent = rsp.bodySize > 0;

  const auto& preview = previewStream.preview();
  if (!preview.empty()) {
    rsp.bodyPreviewHex = formatHexPreview(preview.data(), preview.size());
    rsp.bodyPreviewAscii = formatAsciiPreview(preview.data(), preview.size());
    rsp.bodyProbeKind = probeBodySignature(preview.data(), preview.size(), rsp.bodyProbeOffset);
  }

  if (wireDebugEnabled()) {
    ZO_LOGI("[WireDbg] Body preview bytes=%u%s", static_cast<unsigned int>(preview.size()),
            (rsp.bodySize > preview.size()) ? " (truncated)" : "");
    if (!preview.empty()) {
      ZO_LOGI("[WireDbg] Body preview HEX: %s", rsp.bodyPreviewHex.c_str());
      ZO_LOGI("[WireDbg] Body preview ASCII: %s", rsp.bodyPreviewAscii.c_str());
      ZO_LOGI("[WireDbg] Body probe: kind=%s offset=%ld", bodyProbeKindText(rsp.bodyProbeKind).c_str(),
              static_cast<long>(rsp.bodyProbeOffset));
    }
  }

  if (rsp.httpOk && rsp.bodyPresent && rsp.headers.timestamp.isEmpty()) {
    rsp.status = TransportStatus::ProtocolError;
    rsp.resultClass = ProtocolResultClass::ProtocolMissingTimestamp;
    rsp.missingRequiredTimestamp = true;
    rsp.errorMessage = "Missing required header: Timestamp";
    ZO_LOGW("Protocol incompatibility: HTTP 200 with body but missing Timestamp header");
    if (wireDebugEnabled()) {
      ZO_LOGW("[WireDbg] Debug probe only: body classified as %s at offset %ld",
              bodyProbeKindText(rsp.bodyProbeKind).c_str(), static_cast<long>(rsp.bodyProbeOffset));
    }
  } else if (rsp.hasNewContent && !rsp.bodyPresent) {
    rsp.status = TransportStatus::ProtocolError;
    rsp.resultClass = ProtocolResultClass::ProtocolBodyMissing;
    rsp.errorMessage = "Timestamp changed but body missing";
  } else if (rsp.httpOk && !rsp.headers.timestamp.isEmpty()) {
    rsp.resultClass = rsp.hasNewContent ? ProtocolResultClass::SuccessChanged
                                        : ProtocolResultClass::SuccessUnchanged;
  }

  client->stop();
  ZO_LOGI("HTTP end: bodyBytes=%u handled=%d", static_cast<unsigned int>(rsp.bodySize),
          rsp.bodyHandled ? 1 : 0);

  return rsp;
}

String ProtocolCompatService::buildMetadataJson(const RequestMetadata& md) const {
  String json;
  json.reserve(640);
  json += "{";
  json += "\"fwVersion\":\"" + jsonEscape(md.fwVersion) + "\",";
  json += "\"apiVersion\":\"" + jsonEscape(md.apiVersion) + "\",";
  json += "\"buildDate\":\"" + jsonEscape(md.buildDate) + "\",";
  json += "\"board\":\"" + jsonEscape(md.board) + "\",";

  json += "\"system\":{";
  if (!md.systemResetReason.isEmpty()) {
    json += "\"resetReason\":\"" + jsonEscape(md.systemResetReason) + "\"";
  }
  json += "},";

  json += "\"network\":{";
  json += "\"ssid\":\"" + jsonEscape(md.wifiSsid) + "\",";
  json += "\"rssi\":" + String(md.rssi) + ",";
  json += "\"mac\":\"" + jsonEscape(md.mac) + "\",";
  json += "\"apRetries\":" + String(md.apRetries) + ",";
  json += "\"ipAddress\":\"" + jsonEscape(md.ipAddress) + "\"";
  json += "},";

  json += "\"display\":{";
  json += "\"type\":\"" + jsonEscape(md.displayType) + "\",";
  json += "\"width\":" + String(md.displayWidth) + ",";
  json += "\"height\":" + String(md.displayHeight) + ",";
  json += "\"colorType\":\"" + jsonEscape(md.displayColorType) + "\"";
  json += "}";

  json += "}";
  return json;
}

bool ProtocolCompatService::validateRequestMetadata(const RequestMetadata& md, const String& apiKey,
                                                    String& validationError) const {
  if (apiKey.length() != 8) {
    validationError = "Invalid API key: must be 8 digits";
    return false;
  }
  for (size_t i = 0; i < apiKey.length(); ++i) {
    if (!isDigit(apiKey[i])) {
      validationError = "Invalid API key: non-digit character";
      return false;
    }
  }
  if (md.board.isEmpty()) {
    validationError = "Invalid request metadata: board is empty";
    return false;
  }
  if (WiFi.status() == WL_CONNECTED && md.ipAddress.isEmpty()) {
    validationError = "Invalid request metadata: network.ipAddress is empty";
    return false;
  }
  if (md.displayWidth == 0) {
    validationError = "Invalid request metadata: display.width must be > 0";
    return false;
  }
  if (md.displayHeight == 0) {
    validationError = "Invalid request metadata: display.height must be > 0";
    return false;
  }
  if (md.displayColorType.isEmpty()) {
    validationError = "Invalid request metadata: display.colorType is empty";
    return false;
  }
  return true;
}

String ProtocolCompatService::joinFieldList(const std::vector<String>& fields) const {
  if (fields.empty()) {
    return "none";
  }
  String out;
  for (size_t i = 0; i < fields.size(); ++i) {
    if (i > 0) {
      out += ",";
    }
    out += fields[i];
  }
  return out;
}

bool ProtocolCompatService::parseBoolHeaderValue(String value) const {
  value.trim();
  value.toLowerCase();
  return value == "1" || value == "true" || value == "yes";
}

bool ProtocolCompatService::wireDebugEnabled() const {
  return cfg_.protocolDebug.wireDebug;
}

BodyProbeKind ProtocolCompatService::probeBodySignature(const uint8_t* data, size_t len,
                                                        int32_t& offset) const {
  offset = -1;
  if (data == nullptr || len == 0) {
    return BodyProbeKind::None;
  }

  for (size_t i = 1; i < len; ++i) {
    const uint8_t a = data[i - 1];
    const uint8_t b = data[i];
    if (a == 0x89 && b == 0x50) {
      offset = static_cast<int32_t>(i - 1);
      return BodyProbeKind::Png;
    }
    if (a == 'Z' && b == '1') {
      offset = static_cast<int32_t>(i - 1);
      return BodyProbeKind::Z1;
    }
    if (a == 'Z' && b == '2') {
      offset = static_cast<int32_t>(i - 1);
      return BodyProbeKind::Z2;
    }
    if (a == 'Z' && b == '3') {
      offset = static_cast<int32_t>(i - 1);
      return BodyProbeKind::Z3;
    }
  }

  if (len >= 5) {
    const bool htmlPrefix = (data[0] == '<') &&
                            ((data[1] == '!' || data[1] == 'h' || data[1] == 'H' || data[1] == '?'));
    if (htmlPrefix) {
      offset = 0;
      return BodyProbeKind::Html;
    }
  }

  size_t printable = 0;
  for (size_t i = 0; i < len; ++i) {
    const uint8_t c = data[i];
    if ((c >= 32 && c <= 126) || c == '\n' || c == '\r' || c == '\t') {
      printable++;
    }
  }

  if (printable >= (len * 8) / 10) {
    offset = 0;
    return BodyProbeKind::Text;
  }

  return BodyProbeKind::Unknown;
}

String ProtocolCompatService::bodyProbeKindText(BodyProbeKind kind) const {
  switch (kind) {
    case BodyProbeKind::Png:
      return "PNG";
    case BodyProbeKind::Z1:
      return "Z1";
    case BodyProbeKind::Z2:
      return "Z2";
    case BodyProbeKind::Z3:
      return "Z3";
    case BodyProbeKind::Html:
      return "HTML";
    case BodyProbeKind::Text:
      return "text";
    case BodyProbeKind::Unknown:
      return "unknown";
    default:
      return "none";
  }
}

}  // namespace zivyobraz::protocol
