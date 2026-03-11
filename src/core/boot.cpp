#include "boot.h"

#include "diagnostics/log.h"
#include "diagnostics/system_info.h"
#include "project_config.h"

#if defined(ESP32)
#include <esp_system.h>
#endif

namespace zivyobraz::core {

void initSerial() {
  Serial.begin(115200);
  delay(100);
}

String resetReason() {
#if defined(ESP32)
  return String(esp_reset_reason());
#else
  return String("N/A");
#endif
}

void logBootBanner(const String& boardName) {
  ZO_LOGI("==============================");
  ZO_LOGI("ZivyObraz TFT Client (scaffold)");
  ZO_LOGI("FW: %s | API: %s", ZO_FW_VERSION, ZO_API_VERSION);
  ZO_LOGI("Build: %s", ZO_BUILD_DATE);
  ZO_LOGI("Board: %s", boardName.c_str());
  ZO_LOGI("Chip: %s", diagnostics::chipModel().c_str());
  ZO_LOGI("Flash: %lu bytes", diagnostics::flashSizeBytes());
  ZO_LOGI("SDK: %s", diagnostics::sdkVersion().c_str());
  ZO_LOGI("Reset reason: %s", resetReason().c_str());
  ZO_LOGI("==============================");
}

}  // namespace zivyobraz::core
