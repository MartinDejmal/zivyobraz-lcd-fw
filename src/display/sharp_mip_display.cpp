#include "sharp_mip_display.h"

#include <SPI.h>
#include <memory>

#include "diagnostics/log.h"

namespace zivyobraz::display {

bool SharpMipDisplay::begin(const DisplayConfig& cfg) {
  cfg_ = cfg;
  // Initialise the hardware SPI bus with runtime-configurable pins before
  // creating the U8G2 instance.  The Sharp LS027B7DH01 has no DC/RS line;
  // U8G2_PIN_NONE is passed for that parameter so U8G2 does not toggle it.
  SPI.begin(cfg_.pins.sclk, -1 /* miso */, cfg_.pins.mosi, -1 /* ss */);

  u8g2_ = std::make_unique<U8G2_LS027B7DH01_400X240_F_4W_HW_SPI>(
      U8G2_R0, static_cast<uint8_t>(cfg_.pins.cs),
      U8X8_PIN_NONE,  // dc: not used by Sharp Memory LCD
      cfg_.pins.rst >= 0 ? static_cast<uint8_t>(cfg_.pins.rst) : U8X8_PIN_NONE);

  if (!u8g2_->begin()) {
    ZO_LOGE("SharpMip: U8G2 begin failed");
    return false;
  }

  // Optional DISP pin (stored in cfg_.pins.dc field).  HIGH = display on.
  if (cfg_.pins.dc >= 0) {
    pinMode(cfg_.pins.dc, OUTPUT);
    digitalWrite(cfg_.pins.dc, HIGH);
  }

  u8g2_->clearBuffer();
  u8g2_->sendBuffer();

  initialized_ = true;
  ZO_LOGI("Sharp LS027B7DH01 initialized %ux%u (pins: mosi=%d sclk=%d cs=%d disp=%d rst=%d)",
          cfg_.width, cfg_.height, cfg_.pins.mosi, cfg_.pins.sclk, cfg_.pins.cs, cfg_.pins.dc,
          cfg_.pins.rst);
  return true;
}

void SharpMipDisplay::clear(uint16_t color565) {
  if (!initialized_) {
    return;
  }
  u8g2_->clearBuffer();  // fills with 0 = white (background)
  if (color565 == 0x0000) {
    // Fill with black
    u8g2_->setDrawColor(1);
    u8g2_->drawBox(0, 0, cfg_.width, cfg_.height);
  }
  u8g2_->sendBuffer();
}

void SharpMipDisplay::drawTestPattern() {
  if (!initialized_) {
    return;
  }
  u8g2_->clearBuffer();
  u8g2_->setDrawColor(1);
  // Two black quadrants (top-left and bottom-right), two white quadrants
  u8g2_->drawBox(0, 0, cfg_.width / 2, cfg_.height / 2);
  u8g2_->drawBox(cfg_.width / 2, cfg_.height / 2, cfg_.width / 2, cfg_.height / 2);
  u8g2_->sendBuffer();
}

void SharpMipDisplay::drawStatusScreen(const StatusSnapshot& status) {
  if (!initialized_) {
    return;
  }
  u8g2_->clearBuffer();
  u8g2_->setFont(u8g2_font_5x7_tr);
  u8g2_->setDrawColor(1);

  constexpr uint8_t kLineH = 9;
  uint8_t y = 7;

  u8g2_->setCursor(2, y);
  u8g2_->printf("FW:%s", status.fwVersion.c_str());
  y += kLineH;

  u8g2_->setCursor(2, y);
  u8g2_->printf("WiFi:%s", status.wifiStatus.c_str());
  y += kLineH;

  if (!status.wifiApSsid.isEmpty()) {
    u8g2_->setCursor(2, y);
    u8g2_->printf("CONFIG REZIM");
    y += kLineH;
    u8g2_->setCursor(2, y);
    u8g2_->printf("AP:%s", status.wifiApSsid.c_str());
    y += kLineH;
    u8g2_->setCursor(2, y);
    u8g2_->printf("PASS:%s", status.wifiApPassword.c_str());
    y += kLineH;
    u8g2_->setCursor(2, y);
    u8g2_->printf("IP:%s", status.wifiPortalIp.c_str());
    y += kLineH;
    u8g2_->setCursor(2, y);
    u8g2_->printf("1) Pripojit WiFi");
    y += kLineH;
    u8g2_->setCursor(2, y);
    u8g2_->printf("2) Otevrit portal");
    y += kLineH;
    u8g2_->setCursor(2, y);
    u8g2_->printf("3) Nastavit domaci");
    u8g2_->sendBuffer();
    return;
  }

  u8g2_->setCursor(2, y);
  u8g2_->printf("API:%s", status.apiKey.c_str());
  y += kLineH;
  u8g2_->setCursor(2, y);
  u8g2_->printf("Host:%s", status.serverHost.c_str());
  y += kLineH;
  u8g2_->setCursor(2, y);
  u8g2_->printf("Proto:%s", status.protocolStatus.c_str());
  y += kLineH;
  u8g2_->setCursor(2, y);
  u8g2_->printf("Dbg:%s missTS:%s", status.protocolDebug.c_str(),
                status.timestampMissing.c_str());
  y += kLineH;
  u8g2_->setCursor(2, y);
  u8g2_->printf("Fmt:%s off:%s", status.detectedFormat.c_str(),
                status.signatureOffset.c_str());
  y += kLineH;
  u8g2_->setCursor(2, y);
  u8g2_->printf("Probe:%s off:%s", status.bodyProbe.c_str(), status.bodyProbeOffset.c_str());
  y += kLineH;
  u8g2_->setCursor(2, y);
  u8g2_->printf("Decode:%s", status.decodeResult.c_str());
  y += kLineH;
  u8g2_->setCursor(2, y);
  u8g2_->printf("Pixels:%s", status.pixelsDecoded.c_str());
  y += kLineH;
  u8g2_->setCursor(2, y);
  u8g2_->printf("StoredTS:%s", status.storedTimestamp.c_str());
  y += kLineH;
  u8g2_->setCursor(2, y);
  u8g2_->printf("CandTS:%s", status.candidateTimestamp.c_str());
  y += kLineH;
  u8g2_->setCursor(2, y);
  u8g2_->printf("Render:%s", status.renderResult.c_str());
  u8g2_->sendBuffer();
}

bool SharpMipDisplay::renderIndexed(const image::IndexedFramebuffer& framebuffer, int8_t rotate,
                                    bool partialRefresh) {
  if (!initialized_) {
    return false;
  }
  (void)partialRefresh;  // TODO: use for backend-specific update optimization.
  ZO_LOGI("Render begin: rotate=%d partialHint=%d", rotate, partialRefresh ? 1 : 0);

  u8g2_->clearBuffer();   // clear to white (background)
  u8g2_->setDrawColor(1);  // draw color = black

  for (uint16_t y = 0; y < framebuffer.height(); ++y) {
    for (uint16_t x = 0; x < framebuffer.width(); ++x) {
      uint8_t index = 0;
      if (!framebuffer.getPixel(x, y, index)) {
        ZO_LOGE("Render failed: framebuffer read error");
        return false;
      }
      if (index == 0) {
        continue;  // index 0 = white (background) — skip drawing
      }

      uint16_t dstX = 0;
      uint16_t dstY = 0;
      if (!mapCoordinates(x, y, rotate, dstX, dstY)) {
        ZO_LOGE("Render failed: invalid rotate=%d", rotate);
        return false;
      }
      u8g2_->drawPixel(dstX, dstY);
    }
  }

  u8g2_->sendBuffer();
  ZO_LOGI("Render end: ok");
  return true;
}

void SharpMipDisplay::setBacklight(bool on) {
  // Sharp Memory LCD is a reflective display with no backlight.
  (void)on;
}

bool SharpMipDisplay::mapCoordinates(uint16_t srcX, uint16_t srcY, int8_t rotate, uint16_t& outX,
                                     uint16_t& outY) const {
  if (srcX >= cfg_.width || srcY >= cfg_.height) {
    return false;
  }

  // Coordinate mapping is kept intentionally consistent with St7789Display.
  // For the primary use case (landscape, rotate=0 or rotate=2) all output
  // coordinates are guaranteed to fall within [0, cfg_.width) × [0, cfg_.height).
  // Rotations 1 and 3 are only well-defined when cfg_.width == cfg_.height (square
  // display) or when the server sends a portrait-sized image (cfg_.width=240,
  // cfg_.height=400).  On this non-square 400×240 display with a landscape-sized
  // framebuffer, U8G2 will silently clip any out-of-range pixel from rotations 1/3.
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
