#pragma once

#include <U8g2lib.h>

#include <memory>

#include "display_hal.h"

namespace zivyobraz::display {

// Sharp LS027B7DH01 400x240 Memory-in-Pixel (MIP) B&W display driver.
// Uses the U8G2 library with hardware SPI on ESP32-C3.
// VCOM is toggled by U8G2 internally on every sendBuffer() call.
//
// Pin mapping note: the SpiPins.dc field is repurposed as the optional DISP
// (display on/off) pin.  Set it to -1 if the DISP line is tied HIGH (always on).
class SharpMipDisplay final : public IDisplay {
 public:
  bool begin(const DisplayConfig& cfg) override;
  void clear(uint16_t color565) override;
  void drawTestPattern() override;
  void drawStatusScreen(const StatusSnapshot& status) override;
  bool renderIndexed(const image::IndexedFramebuffer& framebuffer, int8_t rotate,
                     bool partialRefresh) override;
  void setBacklight(bool on) override;

 private:
  bool mapCoordinates(uint16_t srcX, uint16_t srcY, int8_t rotate, uint16_t& outX,
                      uint16_t& outY) const;

  DisplayConfig cfg_{};
  std::unique_ptr<U8G2_LS027B7DH01_400X240_F_4W_HW_SPI> u8g2_{};
  bool initialized_{false};
};

}  // namespace zivyobraz::display
