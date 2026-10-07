#pragma once

#include "Config.h"

// Every parameter is stored in NVS as text under its own name, so loading goes
// through the same validation as the CLI.
class ConfigStore {
 public:
  // Keeps the current value of any key that is missing or invalid.
  bool load(ConfigValues& values);
  bool save(const ConfigValues& values);
  bool erase();
};
