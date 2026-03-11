#include "indexed_framebuffer.h"

#include <algorithm>

namespace zivyobraz::image {

bool IndexedFramebuffer::resize(uint16_t width, uint16_t height) {
  width_ = width;
  height_ = height;
  pixels_.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
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
