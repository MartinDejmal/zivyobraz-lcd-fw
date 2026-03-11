#pragma once

#include <Arduino.h>

#include <vector>

#include "indexed_framebuffer.h"

namespace zivyobraz::image {

enum class ImageFormat : uint8_t {
  Unknown,
  Png,
  Z1,
  Z2,
  Z3,
};

enum class DecodeStatus : uint8_t {
  Ok,
  UnknownFormat,
  RecognizedNotImplemented,
  InvalidData,
  TruncatedData,
  Overflow,
  UnsupportedColor,
  FramebufferError,
};

class IByteStream {
 public:
  virtual ~IByteStream() = default;
  virtual int readByte() = 0;
};

struct DecodeResult {
  ImageFormat format{ImageFormat::Unknown};
  DecodeStatus status{DecodeStatus::UnknownFormat};
  bool success{false};
  bool recognizedButNotImplemented{false};
  size_t signatureOffset{0};
  size_t pixelsDecoded{0};
  size_t bytesConsumed{0};
  String errorMessage;
};

class ImageDecoderFacade {
 public:
  DecodeResult decode(IByteStream& source, IndexedFramebuffer& framebuffer, size_t probeLimit) const;

 private:
  DecodeResult decodeZ1(IByteStream& source, IndexedFramebuffer& framebuffer) const;
  DecodeResult decodeZ2(IByteStream& source, IndexedFramebuffer& framebuffer) const;
  DecodeResult decodeZ3(IByteStream& source, IndexedFramebuffer& framebuffer) const;
};

}  // namespace zivyobraz::image
