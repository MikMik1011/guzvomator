#include "MqttTransport.h"

#include <Arduino.h>
#include <esp_crt_bundle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <mqtt_client.h>

#include <atomic>

namespace {
constexpr int kQos = 1;
constexpr uint16_t kKeepaliveS = 30;
constexpr int kNetworkTimeoutMs = 5000;
constexpr uint32_t kConnectTimeoutMs = 10000;  // covers a TLS handshake
constexpr uint32_t kAckTimeoutMs = 5000;

constexpr EventBits_t kConnected = BIT0;
constexpr EventBits_t kRefused = BIT1;
constexpr EventBits_t kTransportError = BIT2;
constexpr EventBits_t kDisconnected = BIT3;
constexpr EventBits_t kAcknowledged = BIT4;

bool wasRefused(const esp_mqtt_event_t& event) {
  return event.error_handle != nullptr &&
         event.error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED;
}
}  // namespace

struct MqttTransport::Impl {
  EventGroupHandle_t events = xEventGroupCreate();
  esp_mqtt_client_handle_t client = nullptr;
  bool started = false;
  std::atomic<bool> connected{false};
  std::atomic<int> lastAcknowledged{0};

  ~Impl() {
    if (client != nullptr) {
      esp_mqtt_client_stop(client);
      esp_mqtt_client_destroy(client);
    }
    vEventGroupDelete(events);
  }

  static void onEvent(void* arg, esp_event_base_t, int32_t id, void* data) {
    Impl& self = *static_cast<Impl*>(arg);
    const auto* event = static_cast<esp_mqtt_event_handle_t>(data);

    switch (id) {
      case MQTT_EVENT_CONNECTED:
        self.connected = true;
        xEventGroupSetBits(self.events, kConnected);
        break;
      case MQTT_EVENT_DISCONNECTED:
        self.connected = false;
        xEventGroupSetBits(self.events, kDisconnected);
        break;
      case MQTT_EVENT_PUBLISHED:
        self.lastAcknowledged = event->msg_id;
        xEventGroupSetBits(self.events, kAcknowledged);
        break;
      case MQTT_EVENT_ERROR:
        xEventGroupSetBits(self.events, wasRefused(*event) ? kRefused
                                                           : kTransportError);
        break;
      default:
        break;
    }
  }

  bool start(const MqttSettings& settings) {
    esp_mqtt_client_config_t config = {};
    config.broker.address.hostname = settings.host;
    config.broker.address.port = settings.port;
    const bool tls = settings.port == kTlsPort;
    config.broker.address.transport =
        tls ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;
    if (tls) config.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;

    config.credentials.client_id = settings.clientId;
    if (settings.user[0] != '\0') {
      config.credentials.username = settings.user;
      config.credentials.authentication.password = settings.pass;
    }
    config.session.keepalive = kKeepaliveS;
    config.network.timeout_ms = kNetworkTimeoutMs;
    config.network.disable_auto_reconnect = true;

    client = esp_mqtt_client_init(&config);
    if (client == nullptr) return false;
    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, onEvent, this);
    started = esp_mqtt_client_start(client) == ESP_OK;
    return started;
  }

  int connect(const MqttSettings& settings) {
    if (!start(settings)) return kErrorStart;

    const EventBits_t bits = xEventGroupWaitBits(
        events, kConnected | kRefused | kTransportError, pdFALSE, pdFALSE,
        pdMS_TO_TICKS(kConnectTimeoutMs));
    if (bits & kConnected) return 0;
    if (bits & kRefused) return kErrorRefused;
    if (bits & kTransportError) return kErrorTransport;
    return kErrorConnectTimeout;
  }

  bool waitForAcknowledgement(int messageId) {
    const TickType_t started = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(kAckTimeoutMs);
    while (lastAcknowledged != messageId) {
      const TickType_t waited = xTaskGetTickCount() - started;
      if (waited >= timeout || !connected) return false;
      xEventGroupWaitBits(events, kAcknowledged | kDisconnected, pdTRUE,
                          pdFALSE, timeout - waited);
    }
    return true;
  }
};

MqttTransport::MqttTransport(const MqttSettings& settings)
    : impl_(new Impl), settings_(settings) {}

MqttTransport::~MqttTransport() = default;

SendOutcome MqttTransport::send(const char* json, size_t len) {
  if (settings_.host[0] == '\0' || settings_.topic[0] == '\0') {
    return {SendResult::Retry, kErrorNoBroker};
  }

  Impl& connection = *impl_;
  if (!connection.started) {
    const int error = connection.connect(settings_);
    if (error != 0) return {SendResult::Retry, error};
  }
  if (!connection.connected) return {SendResult::Retry, kErrorConnectionLost};

  const int messageId = esp_mqtt_client_publish(
      connection.client, settings_.topic, json, static_cast<int>(len), kQos, 0);
  if (messageId <= 0) return {SendResult::Retry, kErrorConnectionLost};

  if (!connection.waitForAcknowledgement(messageId)) {
    return {SendResult::Retry, kErrorNotAcknowledged};
  }
  return {SendResult::Sent, 0};
}

void MqttTransport::describe(const SendOutcome& outcome, char* out,
                             size_t cap) const {
  switch (outcome.detail) {
    case kErrorNoBroker:
      snprintf(out, cap, "mqtt_host or mqtt_topic is not set");
      break;
    case kErrorStart:
      snprintf(out, cap, "mqtt client did not start");
      break;
    case kErrorConnectTimeout:
      snprintf(out, cap, "mqtt broker did not answer");
      break;
    case kErrorRefused:
      snprintf(out, cap, "mqtt broker refused the login");
      break;
    case kErrorTransport:
      snprintf(out, cap, "mqtt connection failed");
      break;
    case kErrorConnectionLost:
      snprintf(out, cap, "mqtt connection lost");
      break;
    case kErrorNotAcknowledged:
      snprintf(out, cap, "mqtt broker did not acknowledge");
      break;
    default:
      snprintf(out, cap, "mqtt error %d", outcome.detail);
  }
}
