#include "WindowAggregator.h"

#include <math.h>

void WindowAggregator::add(uint64_t deviceHash, int8_t rssi) {
  RssiStats& stats = devices_[deviceHash];
  stats.sum += rssi;
  stats.count++;
}

ScanResult WindowAggregator::summarize(int8_t rssiMin) const {
  ScanResult result;
  result.uniqueDevices = devices_.size();

  float meanSum = 0;
  for (const auto& entry : devices_) {
    const float mean = entry.second.mean();
    meanSum += mean;
    if (mean >= rssiMin) result.uniqueDevicesAboveRssi++;
  }
  if (!devices_.empty()) {
    result.avgRssi = static_cast<int8_t>(lroundf(meanSum / devices_.size()));
  }
  return result;
}

void WindowAggregator::clear() { devices_.clear(); }
