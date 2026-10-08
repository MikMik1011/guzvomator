#pragma once
#include <stdint.h>

// 2020-01-01 UTC, the same bound the backend uses to reject unsynced clocks
constexpr int64_t kMinPlausibleEpoch = 1'577'836'800;

inline bool isPlausibleEpoch(int64_t epochSeconds) {
  return epochSeconds >= kMinPlausibleEpoch;
}

// Retry schedule: the wait doubles after each failure, up to maxMs.
class Backoff {
 public:
  constexpr Backoff(uint32_t initialMs, uint32_t maxMs)
      : initialMs_(initialMs), maxMs_(maxMs), delayMs_(initialMs) {}

  // Safe across the 49-day wrap of millis().
  bool due(uint32_t nowMs) const {
    return !waiting_ || static_cast<int32_t>(nowMs - retryAtMs_) >= 0;
  }

  void failed(uint32_t nowMs) {
    retryAtMs_ = nowMs + delayMs_;
    waiting_ = true;
    delayMs_ = delayMs_ > maxMs_ / 2 ? maxMs_ : delayMs_ * 2;
  }

  void reset() {
    waiting_ = false;
    delayMs_ = initialMs_;
  }

 private:
  uint32_t initialMs_;
  uint32_t maxMs_;
  uint32_t delayMs_;
  uint32_t retryAtMs_ = 0;
  bool waiting_ = false;
};
