#pragma once

#include <Arduino.h>

#include "log_buffer.h"

namespace zivyobraz::diagnostics {

// Writes a formatted log line to Serial and to the web debug ring buffer.
void logLine(char level, const char* fmt, ...);

}  // namespace zivyobraz::diagnostics

#define ZO_LOGI(fmt, ...) zivyobraz::diagnostics::logLine('I', fmt, ##__VA_ARGS__)
#define ZO_LOGW(fmt, ...) zivyobraz::diagnostics::logLine('W', fmt, ##__VA_ARGS__)
#define ZO_LOGE(fmt, ...) zivyobraz::diagnostics::logLine('E', fmt, ##__VA_ARGS__)

