#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#include <memory>

#include "display_hal.h"
#include "image/image_decoder.h"

namespace zivyobraz::display {

class St7789Display final : public IDisplay, public image::PixelSink {
 public:
  bool begin(const DisplayConfig& cfg) override;
  void clear(uint16_t color565) override;
  void drawTestPattern() override;
  void drawStatusScreen(const String& fwVersion, const String& wifiStatus,
                        const String& protocolStatus) override;
  void setBacklight(bool on) override;

  void setPixel(uint16_t x, uint16_t y, uint16_t color565) override;
  uint16_t width() const override;
  uint16_t height() const override;

 private:
  DisplayConfig cfg_{};
  std::unique_ptr<Adafruit_ST7789> tft_{};
  bool initialized_{false};
};

}  // namespace zivyobraz::display
