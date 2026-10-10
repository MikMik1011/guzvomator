#pragma once
#include <stddef.h>

enum class SendResult {
  Sent,      // the backend stored it, or already had it
  Retry,     // might work later: no network, timeout, server error, bad key
  Rejected,  // the backend refuses the payload itself; retrying cannot help
};

struct SendOutcome {
  SendResult result = SendResult::Retry;
  int detail = 0;  // transport specific: an HTTP status, or a negative error
};

class ITransport {
 public:
  virtual ~ITransport() = default;
  virtual SendOutcome send(const char* json, size_t len) = 0;

  // A short human readable form of an outcome this transport produced.
  virtual void describe(const SendOutcome& outcome, char* out,
                        size_t cap) const = 0;
};
