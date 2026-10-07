#include "DeviceConfig.h"

#include <Arduino.h>

void DeviceConfig::begin() {
  std::lock_guard<std::mutex> lock(mutex_);
  store_.load(values_);
  applyDefaultDeviceId();
  dirty_ = false;
}

ConfigValues DeviceConfig::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return values_;
}

SetStatus DeviceConfig::set(const char* name, const char* text) {
  const ParamSpec* spec = findParam(name);
  if (spec == nullptr) return SetStatus::UnknownParameter;

  std::lock_guard<std::mutex> lock(mutex_);
  const SetStatus status = applyText(*spec, values_, text);
  if (status == SetStatus::Ok) dirty_ = true;
  return status;
}

bool DeviceConfig::get(const char* name, char* out, size_t cap) const {
  const ParamSpec* spec = findParam(name);
  if (spec == nullptr) return false;

  std::lock_guard<std::mutex> lock(mutex_);
  formatValue(*spec, values_, out, cap);
  return true;
}

bool DeviceConfig::save() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!store_.save(values_)) return false;
  dirty_ = false;
  version_++;
  return true;
}

void DeviceConfig::reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  store_.erase();
  values_ = ConfigValues();
  applyDefaultDeviceId();
  dirty_ = false;
  version_++;
}

bool DeviceConfig::hasUnsavedChanges() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dirty_;
}

uint32_t DeviceConfig::version() const { return version_; }

void DeviceConfig::applyDefaultDeviceId() {
  if (values_.deviceId[0] != '\0') return;

  const uint64_t mac = ESP.getEfuseMac();
  snprintf(values_.deviceId, sizeof(values_.deviceId), "guzvo-%02x%02x%02x",
           static_cast<unsigned>((mac >> 24) & 0xff),
           static_cast<unsigned>((mac >> 32) & 0xff),
           static_cast<unsigned>((mac >> 40) & 0xff));
}
