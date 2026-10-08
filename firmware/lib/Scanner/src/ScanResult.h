#pragma once
#include <stdint.h>

// Aggregates only; no per-device data.
struct ScanResult {
  uint16_t windowS = 0;
  uint16_t uniqueDevices = 0;
  uint16_t uniqueDevicesAboveRssi = 0;
  int8_t avgRssi = 0;
};
