#pragma once
#include <stddef.h>
#include <stdint.h>

#include "ITransport.h"
#include "NetPolicy.h"
#include "Reading.h"
#include "ReadingQueue.h"

// Queues finished readings and delivers them oldest first. Meant for one task;
// other tasks may read the counters for display.
class Reporter {
 public:
  struct Stats {
    uint32_t sent = 0;
    uint32_t rejected = 0;
    uint32_t dropped = 0;  // pushed out of a full queue
  };

  struct FlushSummary {
    size_t sent = 0;
    size_t rejected = 0;
    bool stalled = false;  // stopped at a failure that is worth retrying
    SendOutcome failure;   // the last failure, when stalled or rejected > 0
  };

  Reporter() : backoff_(kInitialRetryMs, kMaxRetryMs) {}

  void submit(const Reading& reading);

  // Sends queued readings until the queue is empty or a retryable failure
  // happens. After such a failure it does nothing until the backoff expires.
  FlushSummary flush(ITransport& transport, uint32_t nowMs);

  size_t queued() const { return queue_.size(); }
  const Stats& stats() const { return stats_; }

 private:
  static constexpr uint32_t kInitialRetryMs = 5000;
  static constexpr uint32_t kMaxRetryMs = 60000;

  ReadingQueue queue_;
  Backoff backoff_;
  Stats stats_;
};
