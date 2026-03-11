#pragma once

#include <Arduino.h>

namespace zivyobraz::core {

void initSerial();
void logBootBanner(const String& boardName);
String resetReason();

}  // namespace zivyobraz::core
