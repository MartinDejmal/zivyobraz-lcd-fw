#pragma once

#include <Arduino.h>

#include "image/indexed_framebuffer.h"
#include "project_config.h"

namespace zivyobraz::display {

struct StatusSnapshot {
  String fwVersion;
  String wifiStatus;
  String apiKey;
  String serverHost;
  String protocolStatus;
  String detectedFormat;
  String signatureOffset;
  String decodeResult;
  String pixelsDecoded;
  String storedTimestamp;
  String candidateTimestamp;
  String renderResult;
};

class IDisplay {
 public:
  virtual ~IDisplay() = default;
  virtual bool begin(const DisplayConfig& cfg) = 0;
  virtual void clear(uint16_t color565) = 0;
  virtual void drawTestPattern() = 0;
  virtual void drawStatusScreen(const StatusSnapshot& status) = 0;
  virtual bool renderIndexed(const image::IndexedFramebuffer& framebuffer, int8_t rotate,
                             bool partialRefresh) = 0;
  virtual void setBacklight(bool on) = 0;
};

}  // namespace zivyobraz::display
