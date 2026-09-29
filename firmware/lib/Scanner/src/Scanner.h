#pragma once
#include <stddef.h>
#include <stdint.h>

// Aggregates only; no per-device data.
struct ScanResult {
  uint16_t windowS = 0;
  uint16_t uniqueDevices = 0;
  uint16_t uniqueDevicesAboveRssi = 0;
  int8_t avgRssi = 0;
};

class Scanner {
 public:
  bool begin();
  // Blocks for windowS seconds. Hashes live in RAM only for this call.
  ScanResult scan(uint16_t windowS, int8_t rssiMin);
  void setSalt(const uint8_t* salt, size_t len);
};
