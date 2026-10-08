#pragma once
#include <stddef.h>
#include <stdint.h>

#include "ISensor.h"
#include "ScanResult.h"

constexpr size_t kDeviceIdCapacity = 32;

// One finished measurement. Self-contained, so it can wait in a queue while
// the configuration changes.
struct Reading {
  char deviceId[kDeviceIdCapacity] = "";
  uint32_t ts = 0;  // UTC epoch seconds
  int8_t rssiMin = 0;
  ScanResult scan;
  EnvReading env;
};
