#pragma once
#include <stddef.h>

#include <memory>

#include "ITransport.h"

// HTTP or HTTPS POST of payloads. An https:// URL is verified against the
// built-in bundle of trusted root certificates, which needs a correct clock.
// Cheap to build, so make one per flush: it keeps a single connection open for
// all the readings sent through it, and only holds pointers to the two
// strings, which must outlive it.
class HttpTransport : public ITransport {
 public:
  static constexpr int kErrorNoEndpoint = -100;
  static constexpr int kErrorTls = -101;

  HttpTransport(const char* url, const char* apiKey);
  ~HttpTransport() override;

  SendOutcome send(const char* json, size_t len) override;
  void describe(const SendOutcome& outcome, char* out,
                size_t cap) const override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  const char* url_;
  const char* apiKey_;
};
