#pragma once
#include <stddef.h>
#include <stdint.h>

constexpr size_t kSweepSize = 6;
constexpr int8_t kSweepRssi[kSweepSize] = {-70, -75, -80, -85, -90, -95};

// Aggregates only; no per-device data.
struct ScanResult {
  uint16_t windowS = 0;
  uint16_t uniqueDevices = 0;
  uint16_t uniqueDevicesAboveRssi = 0;
  int8_t avgRssi = 0;
  // Unique devices at or above each kSweepRssi threshold, for choosing rssiMin.
  uint16_t sweepCounts[kSweepSize] = {};
};
