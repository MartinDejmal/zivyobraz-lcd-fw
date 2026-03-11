#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#include <memory>

#include "display_hal.h"

namespace zivyobraz::display {

class St7789Display final : public IDisplay {
 public:
  bool begin(const DisplayConfig& cfg) override;
  void clear(uint16_t color565) override;
  void drawTestPattern() override;
  void drawStatusScreen(const StatusSnapshot& status) override;
  bool renderIndexed(const image::IndexedFramebuffer& framebuffer, int8_t rotate,
                     bool partialRefresh) override;
  void setBacklight(bool on) override;

 private:
  uint16_t mapColorIndex(uint8_t colorIndex) const;
  bool mapCoordinates(uint16_t srcX, uint16_t srcY, int8_t rotate, uint16_t& outX,
                      uint16_t& outY) const;

  DisplayConfig cfg_{};
  std::unique_ptr<Adafruit_ST7789> tft_{};
  bool initialized_{false};
};

}  // namespace zivyobraz::display
