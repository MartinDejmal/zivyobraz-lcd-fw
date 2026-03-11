#include "image_decoder.h"

#include "diagnostics/log.h"

namespace zivyobraz::image {

ImageFormat ImageDecoderFacade::detectFormat(const uint8_t* data, size_t len) const {
  if (data == nullptr || len < 2) {
    return ImageFormat::Unknown;
  }

  if (len >= 8 && data[0] == 0x89 && data[1] == 0x50) {
    return ImageFormat::Png;
  }

  if (data[0] == 'Z' && data[1] == '1') {
    return ImageFormat::Z1;
  }
  if (data[0] == 'Z' && data[1] == '2') {
    return ImageFormat::Z2;
  }
  if (data[0] == 'Z' && data[1] == '3') {
    return ImageFormat::Z3;
  }
  return ImageFormat::Unknown;
}

bool ImageDecoderFacade::decodeToSink(const uint8_t* data, size_t len, PixelSink& sink) {
  (void)sink;
  const ImageFormat fmt = detectFormat(data, len);
  // TODO: dispatch to PNG/Z1/Z2/Z3 decoders.
  ZO_LOGW("decodeToSink stub, detected format=%d", static_cast<int>(fmt));
  return false;
}

}  // namespace zivyobraz::image
