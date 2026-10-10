#pragma once
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "ITransport.h"

// Where to publish. Port 8883 means TLS, verified against the built-in bundle
// of trusted root certificates, which needs a correct clock. The strings must
// outlive the transport.
struct MqttSettings {
  const char* host;
  uint16_t port;
  const char* user;
  const char* pass;
  const char* clientId;
  const char* topic;
};

// Publishes each payload with QoS 1 and reports it as sent once the broker has
// acknowledged it. Connects on the first send and disconnects when destroyed,
// so make one per flush.
class MqttTransport : public ITransport {
 public:
  static constexpr uint16_t kTlsPort = 8883;
  static constexpr int kErrorNoBroker = -200;
  static constexpr int kErrorStart = -201;
  static constexpr int kErrorConnectTimeout = -202;
  static constexpr int kErrorRefused = -203;
  static constexpr int kErrorTransport = -204;
  static constexpr int kErrorConnectionLost = -205;
  static constexpr int kErrorNotAcknowledged = -206;

  explicit MqttTransport(const MqttSettings& settings);
  ~MqttTransport() override;

  SendOutcome send(const char* json, size_t len) override;
  void describe(const SendOutcome& outcome, char* out,
                size_t cap) const override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  MqttSettings settings_;
};
