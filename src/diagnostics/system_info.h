#pragma once

#include <Arduino.h>

namespace zivyobraz::diagnostics {

String chipModel();
uint32_t flashSizeBytes();
String sdkVersion();

}  // namespace zivyobraz::diagnostics
