#pragma once

#include "project_config.h"

namespace zivyobraz::config {

class ConfigManager {
 public:
  void begin();
  const RuntimeConfig& get() const;
  bool load();
  bool save();

  const String& apiKey() const;
  const String& lastTimestamp() const;
  void setLastTimestamp(const String& timestamp);

 private:
  RuntimeConfig cfg_{};
  void loadDefaults();
  bool isValidApiKey(const String& apiKey) const;
  String generateApiKey() const;
};

}  // namespace zivyobraz::config
