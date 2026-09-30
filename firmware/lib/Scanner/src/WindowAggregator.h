#pragma once
#include <stdint.h>

#include <unordered_map>

#include "ScanResult.h"

class WindowAggregator {
 public:
  void add(uint64_t deviceHash, int8_t rssi);
  // A device counts as above the threshold when its own mean RSSI is.
  ScanResult summarize(int8_t rssiMin) const;
  void clear();

 private:
  struct RssiStats {
    int32_t sum = 0;
    uint16_t count = 0;
    float mean() const { return static_cast<float>(sum) / count; }
  };

  std::unordered_map<uint64_t, RssiStats> devices_;
};
