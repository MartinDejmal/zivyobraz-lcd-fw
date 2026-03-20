#include "st7789_display.h"

#include <memory>

#include "diagnostics/log.h"

namespace zivyobraz::display {

bool St7789Display::begin(const DisplayConfig& cfg) {
  cfg_ = cfg;
  // TFT_eSPI uses compile-time pin configuration (TFT_MOSI, TFT_SCLK, TFT_CS,
  // TFT_DC, TFT_RST) defined via build flags in platformio.ini.
  // The SPI bus and display reset are handled internally by TFT_eSPI::init().
  // Only cfg_.pins.bl (backlight) and cfg_.rotation remain runtime-configurable.
  tft_ = std::make_unique<TFT_eSPI>();
  tft_->init();
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

  tft_->fillScreen(TFT_BLACK);
  tft_->fillRect(0, 0, cfg_.width / 2, cfg_.height / 2, TFT_RED);
  tft_->fillRect(cfg_.width / 2, 0, cfg_.width / 2, cfg_.height / 2, TFT_GREEN);
  tft_->fillRect(0, cfg_.height / 2, cfg_.width / 2, cfg_.height / 2, TFT_BLUE);
  tft_->fillRect(cfg_.width / 2, cfg_.height / 2, cfg_.width / 2, cfg_.height / 2, TFT_YELLOW);
}

void St7789Display::drawStatusScreen(const StatusSnapshot& status) {
  if (!initialized_) {
    return;
  }

  tft_->fillScreen(TFT_BLACK);
  tft_->setTextColor(TFT_CYAN);
  tft_->setTextSize(1);
  tft_->setCursor(2, 2);
  tft_->printf("FW:%s", status.fwVersion.c_str());

  tft_->setTextColor(TFT_GREEN);
  tft_->setCursor(2, 14);
  tft_->printf("WiFi:%s", status.wifiStatus.c_str());

  if (!status.wifiApSsid.isEmpty()) {
    tft_->setTextColor(TFT_YELLOW);
    tft_->setCursor(2, 26);
    tft_->printf("CONFIG REZIM");
    tft_->setCursor(2, 38);
    tft_->printf("AP:%s", status.wifiApSsid.c_str());
    tft_->setCursor(2, 50);
    tft_->printf("PASS:%s", status.wifiApPassword.c_str());
    tft_->setCursor(2, 62);
    tft_->printf("IP:%s", status.wifiPortalIp.c_str());
    tft_->setCursor(2, 74);
    tft_->printf("1) Pripojit WiFi");
    tft_->setCursor(2, 86);
    tft_->printf("2) Otevrit portal");
    tft_->setCursor(2, 98);
    tft_->printf("3) Nastavit domaci");
    return;
  }

  tft_->setTextColor(TFT_WHITE);
  tft_->setCursor(2, 26);
  tft_->printf("API:%s", status.apiKey.c_str());
  tft_->setCursor(2, 38);
  tft_->printf("Host:%s", status.serverHost.c_str());

  tft_->setTextColor(TFT_YELLOW);
  tft_->setCursor(2, 50);
  tft_->printf("Proto:%s", status.protocolStatus.c_str());
  tft_->setCursor(2, 62);
  tft_->printf("Dbg:%s missTS:%s", status.protocolDebug.c_str(), status.timestampMissing.c_str());

  tft_->setTextColor(TFT_MAGENTA);
  tft_->setCursor(2, 74);
  tft_->printf("Fmt:%s off:%s", status.detectedFormat.c_str(), status.signatureOffset.c_str());
  tft_->setCursor(2, 86);
  tft_->printf("Probe:%s off:%s", status.bodyProbe.c_str(), status.bodyProbeOffset.c_str());

  tft_->setTextColor(tft_->color565(255, 180, 60));
  tft_->setCursor(2, 98);
  tft_->printf("Decode:%s", status.decodeResult.c_str());
  tft_->setCursor(2, 110);
  tft_->printf("Pixels:%s", status.pixelsDecoded.c_str());

  tft_->setTextColor(TFT_CYAN);
  tft_->setCursor(2, 122);
  tft_->printf("StoredTS:%s", status.storedTimestamp.c_str());
  tft_->setCursor(2, 134);
  tft_->printf("CandTS:%s", status.candidateTimestamp.c_str());

  tft_->setTextColor(tft_->color565(255, 165, 0));
  tft_->setCursor(2, 146);
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
      return TFT_WHITE;
    case 1:
      return TFT_BLACK;
    case 2:
      return TFT_RED;
    case 3:
      return TFT_YELLOW;
    case 4:
      return TFT_GREEN;
    case 5:
      return TFT_BLUE;
    case 6:
      return tft_->color565(255, 165, 0);
    default:
      return TFT_BLACK;
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
