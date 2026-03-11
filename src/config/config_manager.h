#pragma once

#include "project_config.h"

namespace zivyobraz::config {

class ConfigManager {
 public:
  void begin();
  const RuntimeConfig& get() const;
  bool load();
  bool save();

 private:
  RuntimeConfig cfg_{};
  void loadDefaults();
};

}  // namespace zivyobraz::config
