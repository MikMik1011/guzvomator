#include "HttpTransport.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <string.h>

#include "HttpStatus.h"

namespace {
constexpr uint16_t kConnectTimeoutMs = 3000;
constexpr uint16_t kReadTimeoutMs = 5000;
constexpr unsigned long kHandshakeTimeoutS = 8;

// Written and read by the one task that sends
char lastTlsError[96] = "";

bool isHttps(const char* url) { return strncmp(url, "https://", 8) == 0; }
}  // namespace

struct HttpTransport::Impl {
  std::unique_ptr<NetworkClient> plainClient;
  std::unique_ptr<NetworkClientSecure> secureClient;
  HTTPClient http;  // declared last, so it is destroyed before the clients
  bool opened = false;

  ~Impl() { http.end(); }

  bool open(const char* url) {
    NetworkClient* client;
    if (isHttps(url)) {
      secureClient.reset(new NetworkClientSecure());
      secureClient->useBuiltinCACertBundle();
      secureClient->setHandshakeTimeout(kHandshakeTimeoutS);
      client = secureClient.get();
    } else {
      plainClient.reset(new NetworkClient());
      client = plainClient.get();
    }

    http.setReuse(true);
    http.setConnectTimeout(kConnectTimeoutMs);
    http.setTimeout(kReadTimeoutMs);
    opened = http.begin(*client, url);
    return opened;
  }

  bool recordTlsError() {
    if (!secureClient) return false;
    char text[sizeof(lastTlsError)];
    if (secureClient->lastError(text, sizeof(text)) == 0) return false;
    strlcpy(lastTlsError, text, sizeof(lastTlsError));
    return true;
  }
};

HttpTransport::HttpTransport(const char* url, const char* apiKey)
    : impl_(new Impl), url_(url), apiKey_(apiKey) {}

HttpTransport::~HttpTransport() = default;

SendOutcome HttpTransport::send(const char* json, size_t len) {
  if (url_[0] == '\0') return {SendResult::Retry, kErrorNoEndpoint};

  Impl& connection = *impl_;
  if (!connection.opened && !connection.open(url_)) {
    return {SendResult::Retry, HTTPC_ERROR_CONNECTION_REFUSED};
  }

  connection.http.addHeader("Content-Type", "application/json");
  if (apiKey_[0] != '\0') connection.http.addHeader("X-API-Key", apiKey_);

  String body;
  body.concat(json, len);
  const int status = connection.http.POST(body);

  if (status < 0 && connection.recordTlsError()) {
    return {SendResult::Retry, kErrorTls};
  }
  return {classifyHttpStatus(status), status};
}

void describeOutcome(const SendOutcome& outcome, char* out, size_t cap) {
  const int detail = outcome.detail;
  if (detail == HttpTransport::kErrorNoEndpoint) {
    snprintf(out, cap, "endpoint_url is not set");
  } else if (detail == HttpTransport::kErrorTls) {
    snprintf(out, cap, "tls error: %s", lastTlsError);
  } else if (detail > 0) {
    snprintf(out, cap, "http %d", detail);
  } else {
    snprintf(out, cap, "%s", HTTPClient::errorToString(detail).c_str());
  }
}
