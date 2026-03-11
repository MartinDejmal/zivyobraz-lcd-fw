#pragma once

#include <Arduino.h>

#include <vector>

namespace zivyobraz::image {

class IndexedFramebuffer {
 public:
  bool resize(uint16_t width, uint16_t height);
  void fill(uint8_t colorIndex);

  bool setPixel(uint16_t x, uint16_t y, uint8_t colorIndex);
  bool getPixel(uint16_t x, uint16_t y, uint8_t& colorIndex) const;

  uint16_t width() const;
  uint16_t height() const;
  size_t pixelCount() const;

 private:
  uint16_t width_{0};
  uint16_t height_{0};
  std::vector<uint8_t> pixels_{};
};

}  // namespace zivyobraz::image
