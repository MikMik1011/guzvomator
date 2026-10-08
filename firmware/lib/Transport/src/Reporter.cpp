#include "Reporter.h"

#include "Payload.h"

void Reporter::submit(const Reading& reading) {
  if (queue_.push(reading)) stats_.dropped++;
}

Reporter::FlushSummary Reporter::flush(ITransport& transport, uint32_t nowMs) {
  FlushSummary summary;
  if (!backoff_.due(nowMs)) return summary;

  while (!queue_.empty()) {
    char payload[kMaxPayloadLen];
    const size_t length = buildPayload(queue_.front(), payload, sizeof(payload));

    SendOutcome outcome;
    if (length == 0) {
      outcome.result = SendResult::Rejected;  // can never be built, so drop it
    } else {
      outcome = transport.send(payload, length);
    }

    switch (outcome.result) {
      case SendResult::Sent:
        queue_.pop();
        stats_.sent++;
        summary.sent++;
        backoff_.reset();
        break;
      case SendResult::Rejected:
        queue_.pop();
        stats_.rejected++;
        summary.rejected++;
        summary.failure = outcome;
        break;
      case SendResult::Retry:
        backoff_.failed(nowMs);
        summary.stalled = true;
        summary.failure = outcome;
        return summary;
    }
  }
  return summary;
}
