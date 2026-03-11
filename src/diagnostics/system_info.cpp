#include "system_info.h"

#include <esp_system.h>

namespace zivyobraz::diagnostics {

String chipModel() {
  return String(ESP.getChipModel());
}

uint32_t flashSizeBytes() {
  return ESP.getFlashChipSize();
}

String sdkVersion() {
  return String(esp_get_idf_version());
}

}  // namespace zivyobraz::diagnostics
