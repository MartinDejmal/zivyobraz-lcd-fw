#include "log.h"

#include <stdarg.h>

namespace zivyobraz::diagnostics {

void logLine(char level, const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  // Keep serial output identical to the original behaviour.
  Serial.printf("[%c] %s\n", level, buf);

  // Also push to ring buffer so the web debug tab can display it.
  char fullBuf[260];
  snprintf(fullBuf, sizeof(fullBuf), "[%c] %s", level, buf);
  gLogBuffer.push(String(fullBuf));
}

}  // namespace zivyobraz::diagnostics
