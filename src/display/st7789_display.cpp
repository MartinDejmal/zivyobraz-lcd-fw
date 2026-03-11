#include "st7789_display.h"

#include <memory>
#include <SPI.h>

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

void St7789Display::drawStatusScreen(const String& fwVersion, const String& wifiStatus,
                                     const String& protocolStatus) {
  if (!initialized_) {
    return;
  }

  tft_->fillScreen(ST77XX_BLACK);
  tft_->setTextColor(ST77XX_WHITE);
  tft_->setTextSize(2);
  tft_->setCursor(10, 20);
  tft_->println("ZivyObraz");
  tft_->println("TFT Client");

  tft_->setTextSize(1);
  tft_->setCursor(10, 80);
  tft_->printf("FW: %s\n", fwVersion.c_str());
  tft_->printf("WiFi: %s\n", wifiStatus.c_str());
  tft_->printf("Protocol: %s\n", protocolStatus.c_str());
}

void St7789Display::setBacklight(bool on) {
  if (cfg_.pins.bl < 0) {
    return;
  }
  digitalWrite(cfg_.pins.bl, on ? HIGH : LOW);
}

void St7789Display::setPixel(uint16_t x, uint16_t y, uint16_t color565) {
  if (!initialized_ || x >= cfg_.width || y >= cfg_.height) {
    return;
  }
  tft_->drawPixel(x, y, color565);
}

uint16_t St7789Display::width() const {
  return cfg_.width;
}

uint16_t St7789Display::height() const {
  return cfg_.height;
}

}  // namespace zivyobraz::display
