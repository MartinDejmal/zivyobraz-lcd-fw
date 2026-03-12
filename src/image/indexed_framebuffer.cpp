#include "indexed_framebuffer.h"

#include <algorithm>
#include <limits>
#include <new>

#include <esp_heap_caps.h>

#include "diagnostics/log.h"

namespace zivyobraz::image {

namespace {
constexpr uint16_t kMaxFramebufferDimension = 1024;
constexpr size_t kMaxFramebufferPixels = static_cast<size_t>(kMaxFramebufferDimension) *
                                         static_cast<size_t>(kMaxFramebufferDimension);
}

bool IndexedFramebuffer::resize(uint16_t width, uint16_t height) {
  if (width == 0 || height == 0) {
    ZO_LOGE("Framebuffer resize rejected: invalid dimensions %ux%u",
            static_cast<unsigned int>(width), static_cast<unsigned int>(height));
    return false;
  }

  // This layer has no reliable access to concrete panel limits, so apply a conservative
  // guard against clearly bogus payload dimensions.
  if (width > kMaxFramebufferDimension || height > kMaxFramebufferDimension) {
    ZO_LOGE("Framebuffer resize rejected: suspicious dimensions %ux%u (max %u)",
            static_cast<unsigned int>(width), static_cast<unsigned int>(height),
            static_cast<unsigned int>(kMaxFramebufferDimension));
    return false;
  }

  const size_t widthSize = static_cast<size_t>(width);
  const size_t heightSize = static_cast<size_t>(height);
  if (widthSize > (std::numeric_limits<size_t>::max() / heightSize)) {
    ZO_LOGE("Framebuffer resize rejected: size overflow for %ux%u",
            static_cast<unsigned int>(width), static_cast<unsigned int>(height));
    return false;
  }

  const size_t requestedPixels = widthSize * heightSize;
  if (requestedPixels > kMaxFramebufferPixels) {
    ZO_LOGE("Framebuffer resize rejected: suspicious pixel count %u for %ux%u",
            static_cast<unsigned int>(requestedPixels), static_cast<unsigned int>(width),
            static_cast<unsigned int>(height));
    return false;
  }

  if (width_ == width && height_ == height && pixels_.size() == requestedPixels) {
    return true;
  }

  ZO_LOGI("Framebuffer resize request %ux%u (pixels=%u), heap free=%u min=%u largest=%u",
          static_cast<unsigned int>(width), static_cast<unsigned int>(height),
          static_cast<unsigned int>(requestedPixels), static_cast<unsigned int>(ESP.getFreeHeap()),
          static_cast<unsigned int>(ESP.getMinFreeHeap()),
          static_cast<unsigned int>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));

  try {
    pixels_.resize(requestedPixels);
  } catch (const std::bad_alloc&) {
    ZO_LOGE("Framebuffer allocation failed for %ux%u (pixels=%u), heap free=%u min=%u largest=%u",
            static_cast<unsigned int>(width), static_cast<unsigned int>(height),
            static_cast<unsigned int>(requestedPixels),
            static_cast<unsigned int>(ESP.getFreeHeap()),
            static_cast<unsigned int>(ESP.getMinFreeHeap()),
            static_cast<unsigned int>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    return false;
  }

  width_ = width;
  height_ = height;
  std::fill(pixels_.begin(), pixels_.end(), 0);
  return pixels_.size() == pixelCount();
}

void IndexedFramebuffer::fill(uint8_t colorIndex) {
  std::fill(pixels_.begin(), pixels_.end(), colorIndex);
}

bool IndexedFramebuffer::setPixel(uint16_t x, uint16_t y, uint8_t colorIndex) {
  if (x >= width_ || y >= height_) {
    return false;
  }
  pixels_[static_cast<size_t>(y) * width_ + x] = colorIndex;
  return true;
}

bool IndexedFramebuffer::getPixel(uint16_t x, uint16_t y, uint8_t& colorIndex) const {
  if (x >= width_ || y >= height_) {
    return false;
  }
  colorIndex = pixels_[static_cast<size_t>(y) * width_ + x];
  return true;
}

uint16_t IndexedFramebuffer::width() const {
  return width_;
}

uint16_t IndexedFramebuffer::height() const {
  return height_;
}

size_t IndexedFramebuffer::pixelCount() const {
  return static_cast<size_t>(width_) * static_cast<size_t>(height_);
}

}  // namespace zivyobraz::image
