#include "image_decoder.h"

#include <vector>

#include "diagnostics/log.h"

namespace zivyobraz::image {

namespace {
constexpr uint8_t kPngSig0 = 0x89;
constexpr uint8_t kPngSig1 = 0x50;

struct ProbeResult {
  ImageFormat format{ImageFormat::Unknown};
  size_t signatureOffset{0};
  std::vector<uint8_t> replayAfterSignature{};
  size_t scannedBytes{0};
  bool found{false};
};

class VectorReplayStream final : public IByteStream {
 public:
  VectorReplayStream(const std::vector<uint8_t>& data, IByteStream& fallback)
      : data_(data), fallback_(fallback) {}

  int readByte() override {
    if (idx_ < data_.size()) {
      return data_[idx_++];
    }
    return fallback_.readByte();
  }

 private:
  const std::vector<uint8_t>& data_;
  IByteStream& fallback_;
  size_t idx_{0};
};

bool isKnownSignature(uint8_t a, uint8_t b, ImageFormat& fmt) {
  if (a == kPngSig0 && b == kPngSig1) {
    fmt = ImageFormat::Png;
    return true;
  }
  if (a == 'Z' && b == '1') {
    fmt = ImageFormat::Z1;
    return true;
  }
  if (a == 'Z' && b == '2') {
    fmt = ImageFormat::Z2;
    return true;
  }
  if (a == 'Z' && b == '3') {
    fmt = ImageFormat::Z3;
    return true;
  }
  return false;
}

ProbeResult probeSignature(IByteStream& source, size_t probeLimit) {
  ProbeResult result;
  std::vector<uint8_t> scanned;
  scanned.reserve(probeLimit);

  while (scanned.size() < probeLimit) {
    const int v = source.readByte();
    if (v < 0) {
      break;
    }
    scanned.push_back(static_cast<uint8_t>(v));
    if (scanned.size() < 2) {
      continue;
    }

    ImageFormat fmt = ImageFormat::Unknown;
    if (isKnownSignature(scanned[scanned.size() - 2], scanned[scanned.size() - 1], fmt)) {
      result.format = fmt;
      result.signatureOffset = scanned.size() - 2;
      result.found = true;
      if (scanned.size() > result.signatureOffset + 2) {
        result.replayAfterSignature.assign(scanned.begin() + result.signatureOffset + 2,
                                           scanned.end());
      }
      break;
    }
  }

  result.scannedBytes = scanned.size();
  return result;
}

DecodeResult decodeRleRuns(IByteStream& source, IndexedFramebuffer& framebuffer, uint8_t colorShift,
                           uint8_t countMask, uint8_t maxColor) {
  DecodeResult result;
  const size_t totalPixels = framebuffer.pixelCount();
  size_t pixelPos = 0;

  while (pixelPos < totalPixels) {
    const int runValue = source.readByte();
    if (runValue < 0) {
      result.status = DecodeStatus::TruncatedData;
      result.errorMessage = "Unexpected EOF in runs";
      return result;
    }
    result.bytesConsumed++;

    const uint8_t raw = static_cast<uint8_t>(runValue);
    const uint8_t color = raw >> colorShift;
    const uint8_t count = raw & countMask;

    if (count == 0) {
      result.status = DecodeStatus::InvalidData;
      result.errorMessage = "Zero-length run";
      return result;
    }
    if (color > maxColor) {
      result.status = DecodeStatus::UnsupportedColor;
      result.errorMessage = "Color index out of range";
      return result;
    }
    if (pixelPos + count > totalPixels) {
      result.status = DecodeStatus::Overflow;
      result.errorMessage = "Run overflows framebuffer";
      return result;
    }

    for (uint8_t i = 0; i < count; ++i) {
      const uint16_t x = static_cast<uint16_t>(pixelPos % framebuffer.width());
      const uint16_t y = static_cast<uint16_t>(pixelPos / framebuffer.width());
      if (!framebuffer.setPixel(x, y, color)) {
        result.status = DecodeStatus::FramebufferError;
        result.errorMessage = "Failed to write framebuffer pixel";
        return result;
      }
      pixelPos++;
      result.pixelsDecoded++;
    }
  }

  result.status = DecodeStatus::Ok;
  result.success = true;
  return result;
}

}  // namespace

DecodeResult ImageDecoderFacade::decode(IByteStream& source, IndexedFramebuffer& framebuffer,
                                        size_t probeLimit) const {
  DecodeResult result;

  const ProbeResult probe = probeSignature(source, probeLimit);
  result.format = probe.format;
  result.signatureOffset = probe.signatureOffset;

  if (!probe.found) {
    result.status = DecodeStatus::UnknownFormat;
    result.errorMessage = "No supported signature in probe window";
    ZO_LOGW("Signature probe failed after %u bytes", static_cast<unsigned int>(probe.scannedBytes));
    return result;
  }

  ZO_LOGI("Signature probe: format=%d offset=%u scanned=%u", static_cast<int>(probe.format),
          static_cast<unsigned int>(probe.signatureOffset),
          static_cast<unsigned int>(probe.scannedBytes));

  if (probe.format == ImageFormat::Png) {
    result.status = DecodeStatus::RecognizedNotImplemented;
    result.recognizedButNotImplemented = true;
    result.errorMessage = "PNG recognized but not implemented yet";
    return result;
  }

  VectorReplayStream decodeStream(probe.replayAfterSignature, source);

  switch (probe.format) {
    case ImageFormat::Z1:
      result = decodeZ1(decodeStream, framebuffer);
      break;
    case ImageFormat::Z2:
      result = decodeZ2(decodeStream, framebuffer);
      break;
    case ImageFormat::Z3:
      result = decodeZ3(decodeStream, framebuffer);
      break;
    default:
      result.status = DecodeStatus::UnknownFormat;
      result.errorMessage = "No decoder available";
      break;
  }

  result.format = probe.format;
  result.signatureOffset = probe.signatureOffset;
  return result;
}

DecodeResult ImageDecoderFacade::decodeZ1(IByteStream& source, IndexedFramebuffer& framebuffer) const {
  DecodeResult result;
  const size_t totalPixels = framebuffer.pixelCount();
  size_t pixelPos = 0;

  while (pixelPos < totalPixels) {
    const int colorValue = source.readByte();
    if (colorValue < 0) {
      result.status = DecodeStatus::TruncatedData;
      result.errorMessage = "Unexpected EOF in Z1 color";
      return result;
    }
    const int countValue = source.readByte();
    if (countValue < 0) {
      result.status = DecodeStatus::TruncatedData;
      result.errorMessage = "Unexpected EOF in Z1 count";
      return result;
    }
    result.bytesConsumed += 2;

    const uint8_t color = static_cast<uint8_t>(colorValue);
    const uint8_t count = static_cast<uint8_t>(countValue);

    if (count == 0) {
      result.status = DecodeStatus::InvalidData;
      result.errorMessage = "Zero-length run";
      return result;
    }
    if (color > 6) {
      result.status = DecodeStatus::UnsupportedColor;
      result.errorMessage = "Color index out of range";
      return result;
    }
    if (pixelPos + count > totalPixels) {
      result.status = DecodeStatus::Overflow;
      result.errorMessage = "Run overflows framebuffer";
      return result;
    }

    for (uint8_t i = 0; i < count; ++i) {
      const uint16_t x = static_cast<uint16_t>(pixelPos % framebuffer.width());
      const uint16_t y = static_cast<uint16_t>(pixelPos / framebuffer.width());
      if (!framebuffer.setPixel(x, y, color)) {
        result.status = DecodeStatus::FramebufferError;
        result.errorMessage = "Failed to write framebuffer pixel";
        return result;
      }
      pixelPos++;
      result.pixelsDecoded++;
    }
  }

  result.status = DecodeStatus::Ok;
  result.success = true;
  return result;
}

DecodeResult ImageDecoderFacade::decodeZ2(IByteStream& source, IndexedFramebuffer& framebuffer) const {
  DecodeResult result = decodeRleRuns(source, framebuffer, 6, 0x3F, 3);
  return result;
}

DecodeResult ImageDecoderFacade::decodeZ3(IByteStream& source, IndexedFramebuffer& framebuffer) const {
  DecodeResult result = decodeRleRuns(source, framebuffer, 5, 0x1F, 6);
  return result;
}

}  // namespace zivyobraz::image
