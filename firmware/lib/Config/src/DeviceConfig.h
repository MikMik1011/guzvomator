#pragma once
#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <mutex>

#include "Config.h"
#include "ConfigStore.h"
#include "ParamCodec.h"

// Thread-safe holder of the configuration. Changes live in RAM until save().
class DeviceConfig {
 public:
  void begin();

  ConfigValues snapshot() const;
  SetStatus set(const char* name, const char* text);
  bool get(const char* name, char* out, size_t cap) const;

  bool save();
  void reset();

  bool hasUnsavedChanges() const;
  // Incremented by every save() and reset(), so tasks can detect a new config.
  uint32_t version() const;

 private:
  void applyDefaultDeviceId();

  mutable std::mutex mutex_;
  ConfigValues values_;
  ConfigStore store_;
  bool dirty_ = false;
  std::atomic<uint32_t> version_{0};
};
