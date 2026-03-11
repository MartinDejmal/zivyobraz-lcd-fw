#pragma once

#include <Arduino.h>

namespace zivyobraz::image {

enum class ImageFormat : uint8_t {
  Unknown,
  Png,
  Z1,
  Z2,
  Z3,
};

struct PixelSink {
  virtual ~PixelSink() = default;
  virtual void setPixel(uint16_t x, uint16_t y, uint16_t color565) = 0;
  virtual uint16_t width() const = 0;
  virtual uint16_t height() const = 0;
};

class IImageDecoder {
 public:
  virtual ~IImageDecoder() = default;
  virtual ImageFormat format() const = 0;
  virtual bool decode(const uint8_t* data, size_t len, PixelSink& sink) = 0;
};

class ImageDecoderFacade {
 public:
  ImageFormat detectFormat(const uint8_t* data, size_t len) const;
  bool decodeToSink(const uint8_t* data, size_t len, PixelSink& sink);
};

}  // namespace zivyobraz::image
