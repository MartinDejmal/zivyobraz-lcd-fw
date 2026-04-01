#pragma once

#include <Arduino.h>

namespace zivyobraz {

#ifndef ZO_FW_VERSION
#define ZO_FW_VERSION "3.0"
#endif

#ifndef ZO_API_VERSION
#define ZO_API_VERSION "3.0"
#endif

#ifndef ZO_BUILD_DATE
#define ZO_BUILD_DATE __DATE__ " " __TIME__
#endif

enum class DisplayType : uint8_t {
  St7789,
  SharpMip,
  Ili9488,
  Unknown,
};

struct SpiPins {
  int8_t mosi;
  int8_t sclk;
  int8_t cs;
  int8_t dc;
  int8_t rst;
  int8_t bl;
};

struct DisplayConfig {
  DisplayType type;
  uint16_t width;
  uint16_t height;
  SpiPins pins;
  uint8_t rotation;
};

struct WifiConfig {
  String ssid;
  String password;
};

struct ServerConfig {
  String host;
  bool useHttps;
  String endpointPath;
};

struct ProtocolVersionConfig {
  String fwVersion;
  String apiVersion;
};


struct ProtocolDebugConfig {
  bool wireDebug{false};
  bool forceTimestampCheckZeroOnMissingTimestamp{false};
};

struct RuntimeConfig {
  String boardName;
  DisplayConfig display;
  WifiConfig wifi;
  ServerConfig server;
  ProtocolVersionConfig versions;
  String apiKey;
  String lastTimestamp;
  ProtocolDebugConfig protocolDebug;
};

}  // namespace zivyobraz
