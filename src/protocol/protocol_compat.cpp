#include "protocol_compat.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "diagnostics/log.h"
#include "project_config.h"

namespace zivyobraz::protocol {

namespace {
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

String trimCopy(String s) {
  s.trim();
  return s;
}
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
  payload.system = "esp32-arduino";
  payload.network = "wifi";
  payload.display = "st7789-240x240";
  payload.sensors = "";
  payload.wifiSsid = WiFi.SSID();
  payload.localIp = WiFi.localIP().toString();
  payload.mac = WiFi.macAddress();
  payload.rssi = WiFi.RSSI();
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
                                                    const String& previousTimestamp) {
  ProtocolResponse rsp;
  rsp.previousTimestamp = previousTimestamp;

  if (WiFi.status() != WL_CONNECTED) {
    rsp.status = TransportStatus::NetworkError;
    rsp.errorMessage = "Wi-Fi disconnected";
    ZO_LOGW("Protocol sync skipped: Wi-Fi disconnected");
    return rsp;
  }

  RequestMetadata metadata = buildMetadataPayload();
  const String body = buildMetadataJson(metadata);
  const String path = cfg_.server.endpointPath + "?timestampCheck=" + String(timestampCheck ? 1 : 0);
  const uint16_t port = cfg_.server.useHttps ? 443 : 80;

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

  String statusLine = client->readStringUntil('\n');
  statusLine.trim();
  if (!statusLine.startsWith("HTTP/1.1")) {
    rsp.status = TransportStatus::ParseError;
    rsp.errorMessage = "Invalid HTTP status line";
    ZO_LOGE("Invalid HTTP response status line: %s", statusLine.c_str());
    client->stop();
    return rsp;
  }

  const int firstSpace = statusLine.indexOf(' ');
  if (firstSpace < 0) {
    rsp.status = TransportStatus::ParseError;
    rsp.errorMessage = "Missing HTTP status code";
    client->stop();
    return rsp;
  }

  rsp.httpStatusCode = statusLine.substring(firstSpace + 1).toInt();
  rsp.transportOk = true;
  rsp.httpOk = (rsp.httpStatusCode == 200);

  ZO_LOGI("HTTP status: %d", rsp.httpStatusCode);
  if (!rsp.httpOk) {
    rsp.status = TransportStatus::HttpError;
    rsp.errorMessage = "Non-200 HTTP status";
  } else {
    rsp.status = TransportStatus::Ok;
  }

  while (client->connected()) {
    String line = client->readStringUntil('\n');
    if (line == "\r" || line.length() == 0) {
      break;
    }
    line.trim();
    const int sep = line.indexOf(':');
    if (sep <= 0) {
      continue;
    }
    String key = line.substring(0, sep);
    String value = trimCopy(line.substring(sep + 1));

    if (key.equalsIgnoreCase("Timestamp")) {
      rsp.headers.timestamp = value;
    } else if (key.equalsIgnoreCase("PreciseSleep")) {
      rsp.headers.preciseSleepSec = static_cast<uint32_t>(value.toInt());
    } else if (key.equalsIgnoreCase("Rotate")) {
      rsp.headers.rotate = static_cast<int8_t>(value.toInt());
    } else if (key.equalsIgnoreCase("PartialRefresh")) {
      rsp.headers.partialRefresh = parseBoolHeaderValue(value);
    } else if (key.equalsIgnoreCase("ShowNoWifiError")) {
      rsp.headers.showNoWifiError = parseBoolHeaderValue(value);
    } else if (key.equalsIgnoreCase("X-OTA-Update")) {
      rsp.headers.otaUpdateUrl = value;
      rsp.hasOtaUpdate = !value.isEmpty();
    }
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

  // For this milestone we consume and count body bytes only.
  // TODO: In the next milestone route this stream directly into format detection/decoder,
  // and commit timestamp only after successful decode + display render.
  while (client->available() || client->connected()) {
    if (!client->available()) {
      delay(5);
      continue;
    }
    const int b = client->read();
    if (b < 0) {
      break;
    }
    rsp.bodySize++;
  }
  rsp.bodyPresent = rsp.bodySize > 0;
  client->stop();
  ZO_LOGI("HTTP end: bodyBytes=%u", static_cast<unsigned int>(rsp.bodySize));

  return rsp;
}

String ProtocolCompatService::buildMetadataJson(const RequestMetadata& md) const {
  String json;
  json.reserve(512);
  json += "{";
  json += "\"fwVersion\":\"" + jsonEscape(md.fwVersion) + "\",";
  json += "\"apiVersion\":\"" + jsonEscape(md.apiVersion) + "\",";
  json += "\"buildDate\":\"" + jsonEscape(md.buildDate) + "\",";
  json += "\"board\":{\"name\":\"" + jsonEscape(md.board) + "\"},";
  json += "\"system\":{\"name\":\"" + jsonEscape(md.system) + "\"},";
  json += "\"network\":{";
  json += "\"type\":\"" + jsonEscape(md.network) + "\",";
  json += "\"ssid\":\"" + jsonEscape(md.wifiSsid) + "\",";
  json += "\"ip\":\"" + jsonEscape(md.localIp) + "\",";
  json += "\"mac\":\"" + jsonEscape(md.mac) + "\",";
  json += "\"rssi\":" + String(md.rssi);
  json += "},";
  json += "\"display\":{\"type\":\"" + jsonEscape(md.display) + "\"}";
  json += "}";
  return json;
}

bool ProtocolCompatService::parseBoolHeaderValue(String value) const {
  value.trim();
  value.toLowerCase();
  return value == "1" || value == "true" || value == "yes";
}

}  // namespace zivyobraz::protocol
