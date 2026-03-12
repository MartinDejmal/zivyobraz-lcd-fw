#include "log_buffer.h"

namespace zivyobraz::diagnostics {

LogBuffer gLogBuffer;

void LogBuffer::push(const String& line) {
  if (lines_.size() >= kMaxLines) {
    lines_.pop_front();
  }
  lines_.push_back(line);
  ++totalPushed_;
}

String LogBuffer::getJson(size_t since, size_t& outNext) const {
  outNext = totalPushed_;

  String json;
  json.reserve(512);
  json = F("{\"lines\":[");

  // Global index of the oldest line currently in the buffer.
  const size_t bufStart = (totalPushed_ >= lines_.size()) ? (totalPushed_ - lines_.size()) : 0;

  bool first = true;
  for (size_t i = 0; i < lines_.size(); ++i) {
    const size_t globalIdx = bufStart + i;
    if (globalIdx < since) {
      continue;
    }

    if (!first) {
      json += ',';
    }
    first = false;

    json += '"';
    const String& line = lines_[i];
    for (size_t j = 0; j < line.length(); ++j) {
      const char c = line[j];
      if (c == '"') {
        json += F("\\\"");
      } else if (c == '\\') {
        json += F("\\\\");
      } else if (c == '\n') {
        json += F("\\n");
      } else if (c == '\r') {
        // skip
      } else {
        json += c;
      }
    }
    json += '"';
  }

  json += F("],\"next\":");
  json += String(outNext);
  json += '}';
  return json;
}

size_t LogBuffer::count() const {
  return lines_.size();
}

}  // namespace zivyobraz::diagnostics
