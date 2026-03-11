#include "st7789_display.h"

#include <SPI.h>
#include <memory>

#include "diagnostics/log.h"

namespace zivyobraz::display {

bool St7789Display::begin(const DisplayConfig& cfg) {
  cfg_ = cfg;
  tft_ = std::make_unique<Adafruit_ST7789>(cfg_.pins.cs, cfg_.pins.dc, cfg_.pins.rst);
  SPI.begin(cfg_.pins.sclk, -1, cfg_.pins.mosi, cfg_.pins.cs);
  tft_->init(cfg_.width, cfg_.height, SPI_MODE3);
  tft_->setRotation(cfg_.rotation);

  if (cfg_.pins.bl >= 0) {
    pinMode(cfg_.pins.bl, OUTPUT);
    digitalWrite(cfg_.pins.bl, HIGH);
  }

  initialized_ = true;
  ZO_LOGI("ST7789 initialized %ux%u (pins: mosi=%d sclk=%d cs=%d dc=%d rst=%d bl=%d)",
          cfg_.width, cfg_.height, cfg_.pins.mosi, cfg_.pins.sclk, cfg_.pins.cs, cfg_.pins.dc,
          cfg_.pins.rst, cfg_.pins.bl);
  return true;
}

void St7789Display::clear(uint16_t color565) {
  if (!initialized_) {
    return;
  }
  tft_->fillScreen(color565);
}

void St7789Display::drawTestPattern() {
  if (!initialized_) {
    return;
  }

  tft_->fillScreen(ST77XX_BLACK);
  tft_->fillRect(0, 0, cfg_.width / 2, cfg_.height / 2, ST77XX_RED);
  tft_->fillRect(cfg_.width / 2, 0, cfg_.width / 2, cfg_.height / 2, ST77XX_GREEN);
  tft_->fillRect(0, cfg_.height / 2, cfg_.width / 2, cfg_.height / 2, ST77XX_BLUE);
  tft_->fillRect(cfg_.width / 2, cfg_.height / 2, cfg_.width / 2, cfg_.height / 2, ST77XX_YELLOW);
}

void St7789Display::drawStatusScreen(const StatusSnapshot& status) {
  if (!initialized_) {
    return;
  }

  tft_->fillScreen(ST77XX_BLACK);
  tft_->setTextColor(ST77XX_CYAN);
  tft_->setTextSize(1);
  tft_->setCursor(2, 2);
  tft_->printf("FW:%s", status.fwVersion.c_str());

  tft_->setTextColor(ST77XX_GREEN);
  tft_->setCursor(2, 16);
  tft_->printf("WiFi:%s", status.wifiStatus.c_str());

  tft_->setTextColor(ST77XX_WHITE);
  tft_->setCursor(2, 30);
  tft_->printf("API:%s", status.apiKey.c_str());
  tft_->setCursor(2, 42);
  tft_->printf("Host:%s", status.serverHost.c_str());

  tft_->setTextColor(ST77XX_YELLOW);
  tft_->setCursor(2, 56);
  tft_->printf("Proto:%s", status.protocolStatus.c_str());
  tft_->setCursor(2, 68);
  tft_->printf("Fmt:%s off:%s", status.detectedFormat.c_str(), status.signatureOffset.c_str());

  tft_->setTextColor(ST77XX_MAGENTA);
  tft_->setCursor(2, 80);
  tft_->printf("Decode:%s", status.decodeResult.c_str());
  tft_->setCursor(2, 92);
  tft_->printf("Pixels:%s", status.pixelsDecoded.c_str());

  tft_->setTextColor(ST77XX_CYAN);
  tft_->setCursor(2, 104);
  tft_->printf("StoredTS:%s", status.storedTimestamp.c_str());
  tft_->setCursor(2, 116);
  tft_->printf("CandTS:%s", status.candidateTimestamp.c_str());

  tft_->setTextColor(tft_->color565(255, 165, 0));
  tft_->setCursor(2, 128);
  tft_->printf("Render:%s", status.renderResult.c_str());
}

bool St7789Display::renderIndexed(const image::IndexedFramebuffer& framebuffer, int8_t rotate,
                                  bool partialRefresh) {
  if (!initialized_) {
    return false;
  }

  (void)partialRefresh;  // TODO: use for backend-specific update optimization.
  ZO_LOGI("Render begin: rotate=%d partialHint=%d", rotate, partialRefresh ? 1 : 0);
  tft_->startWrite();

  for (uint16_t y = 0; y < framebuffer.height(); ++y) {
    for (uint16_t x = 0; x < framebuffer.width(); ++x) {
      uint8_t index = 0;
      if (!framebuffer.getPixel(x, y, index)) {
        tft_->endWrite();
        ZO_LOGE("Render failed: framebuffer read error");
        return false;
      }
      uint16_t dstX = 0;
      uint16_t dstY = 0;
      if (!mapCoordinates(x, y, rotate, dstX, dstY)) {
        tft_->endWrite();
        ZO_LOGE("Render failed: invalid rotate=%d", rotate);
        return false;
      }
      tft_->writePixel(dstX, dstY, mapColorIndex(index));
    }
  }

  tft_->endWrite();
  ZO_LOGI("Render end: ok");
  return true;
}

void St7789Display::setBacklight(bool on) {
  if (cfg_.pins.bl < 0) {
    return;
  }
  digitalWrite(cfg_.pins.bl, on ? HIGH : LOW);
}

uint16_t St7789Display::mapColorIndex(uint8_t colorIndex) const {
  switch (colorIndex) {
    case 0:
      return ST77XX_WHITE;
    case 1:
      return ST77XX_BLACK;
    case 2:
      return ST77XX_RED;
    case 3:
      return ST77XX_YELLOW;
    case 4:
      return ST77XX_GREEN;
    case 5:
      return ST77XX_BLUE;
    case 6:
      return tft_->color565(255, 165, 0);
    default:
      return ST77XX_BLACK;
  }
}

bool St7789Display::mapCoordinates(uint16_t srcX, uint16_t srcY, int8_t rotate, uint16_t& outX,
                                   uint16_t& outY) const {
  if (srcX >= cfg_.width || srcY >= cfg_.height) {
    return false;
  }

  switch (rotate) {
    case 1:
      outX = static_cast<uint16_t>(cfg_.width - 1 - srcY);
      outY = srcX;
      return true;
    case 2:
      outX = static_cast<uint16_t>(cfg_.width - 1 - srcX);
      outY = static_cast<uint16_t>(cfg_.height - 1 - srcY);
      return true;
    case 3:
      outX = srcY;
      outY = static_cast<uint16_t>(cfg_.height - 1 - srcX);
      return true;
    case -1:
    case 0:
      outX = srcX;
      outY = srcY;
      return true;
    default:
      outX = srcX;
      outY = srcY;
      return false;
  }
}

}  // namespace zivyobraz::display
