#include "Payload.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {

class Appender {
 public:
  Appender(char* buf, size_t cap) : buf_(buf), cap_(cap) {}

  void add(const char* format, ...) __attribute__((format(printf, 2, 3))) {
    if (failed_) return;

    va_list args;
    va_start(args, format);
    const int written =
        vsnprintf(buf_ + length_, cap_ - length_, format, args);
    va_end(args);

    if (written < 0 || static_cast<size_t>(written) >= cap_ - length_) {
      failed_ = true;
      return;
    }
    length_ += written;
  }

  bool failed() const { return failed_; }
  size_t length() const { return length_; }

 private:
  char* buf_;
  size_t cap_;
  size_t length_ = 0;
  bool failed_ = false;
};

bool isPlainIdentifier(const char* text) {
  const size_t length = strlen(text);
  if (length == 0 || length >= kDeviceIdCapacity) return false;
  for (size_t i = 0; i < length; i++) {
    const unsigned char c = text[i];
    if (!isalnum(c) && c != '-' && c != '_') return false;
  }
  return true;
}

void addOptional(Appender& json, const char* name, float value) {
  if (!isnan(value)) json.add(",\"%s\":%.1f", name, static_cast<double>(value));
}

}  // namespace

size_t buildPayload(const Reading& reading, char* buf, size_t cap) {
  if (cap > 0) buf[0] = '\0';
  if (!isPlainIdentifier(reading.deviceId)) return 0;

  Appender json(buf, cap);
  json.add(
      "{\"v\":%d,\"device_id\":\"%s\",\"ts\":%lu,\"scan_window_s\":%u,"
      "\"rssi_min\":%d,\"devices\":%u,\"devices_above_rssi\":%u,"
      "\"avg_rssi\":%d",
      kPayloadVersion, reading.deviceId, static_cast<unsigned long>(reading.ts),
      static_cast<unsigned>(reading.scan.windowS),
      static_cast<int>(reading.rssiMin),
      static_cast<unsigned>(reading.scan.uniqueDevices),
      static_cast<unsigned>(reading.scan.uniqueDevicesAboveRssi),
      static_cast<int>(reading.scan.avgRssi));
  addOptional(json, "temp_c", reading.env.temperatureC);
  addOptional(json, "humidity_pct", reading.env.humidityPct);
  addOptional(json, "lux", reading.env.illuminanceLux);
  json.add("}");

  if (json.failed()) {
    if (cap > 0) buf[0] = '\0';
    return 0;
  }
  return json.length();
}
