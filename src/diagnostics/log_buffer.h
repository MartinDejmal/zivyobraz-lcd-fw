#pragma once

#include <Arduino.h>

#include <deque>

namespace zivyobraz::diagnostics {

// Circular ring buffer holding the most recent log lines for the web debug tab.
class LogBuffer {
 public:
  static constexpr size_t kMaxLines = 100;

  void push(const String& line);

  // Returns a JSON object { "lines": [...], "next": N }.
  // Only lines with global index >= since are included.
  // outNext is set to the current total-pushed count (use as the next "since").
  String getJson(size_t since, size_t& outNext) const;

  size_t count() const;

 private:
  std::deque<String> lines_;
  size_t totalPushed_{0};
};

extern LogBuffer gLogBuffer;

}  // namespace zivyobraz::diagnostics
