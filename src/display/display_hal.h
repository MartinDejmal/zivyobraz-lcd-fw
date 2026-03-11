#pragma once

#include <Arduino.h>

#include "project_config.h"

namespace zivyobraz::display {

class IDisplay {
 public:
  virtual ~IDisplay() = default;
  virtual bool begin(const DisplayConfig& cfg) = 0;
  virtual void clear(uint16_t color565) = 0;
  virtual void drawTestPattern() = 0;
  virtual void drawStatusScreen(const String& fwVersion, const String& wifiStatus,
                                const String& protocolStatus) = 0;
  virtual void setBacklight(bool on) = 0;
};

}  // namespace zivyobraz::display
