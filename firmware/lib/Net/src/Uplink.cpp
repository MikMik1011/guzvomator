#include "Uplink.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <WiFiMulti.h>
#include <string.h>
#include <time.h>

#include <atomic>
#include <mutex>

#include "NetPolicy.h"

namespace {
constexpr uint32_t kInitialBackoffMs = 5000;
constexpr uint32_t kMaxBackoffMs = 60000;
constexpr uint32_t kConnectTimeoutMs = 8000;

const char* securityName(wifi_auth_mode_t mode) {
  switch (mode) {
    case WIFI_AUTH_OPEN:
      return "open";
    case WIFI_AUTH_WEP:
      return "wep";
    case WIFI_AUTH_WPA_PSK:
      return "wpa";
    case WIFI_AUTH_WPA2_PSK:
      return "wpa2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "wpa/wpa2";
    case WIFI_AUTH_WPA3_PSK:
      return "wpa3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "wpa2/wpa3";
    case WIFI_AUTH_ENTERPRISE:
      return "enterprise";
    default:
      return "other";
  }
}

// Set by SNTP itself, so a time left over from before a soft reset doesn't count
std::atomic<bool> sntpHasSetTheClock{false};

void onSntpSync(struct timeval*) { sntpHasSetTheClock = true; }

bool isSaved(const ConfigValues& values, const char* ssid) {
  for (const WifiProfile& profile : values.wifi) {
    if (profile.ssid[0] != '\0' && strcmp(profile.ssid, ssid) == 0) return true;
  }
  return false;
}
}  // namespace

struct Uplink::Impl {
  explicit Impl(DeviceConfig& deviceConfig)
      : config(deviceConfig), backoff(kInitialBackoffMs, kMaxBackoffMs) {}

  DeviceConfig& config;
  std::mutex radioMutex;  // one scan or connect attempt at a time
  WiFiMulti wifiMulti;
  Backoff backoff;
  uint32_t appliedVersion = 0;
  bool configApplied = false;
  bool hasProfiles = false;
  bool sntpStarted = false;
  bool wasConnected = false;
  // lwIP keeps the pointer it is given, so the name must outlive the call
  char ntpServer[sizeof(ConfigValues::ntpServer)] = "";
  std::atomic<bool> clockSynced{false};

  void applyConfig() {
    const ConfigValues values = config.snapshot();

    std::lock_guard<std::mutex> lock(radioMutex);
    wifiMulti.APlistClean();
    hasProfiles = false;
    for (const WifiProfile& profile : values.wifi) {
      if (profile.ssid[0] == '\0') continue;
      wifiMulti.addAP(profile.ssid, profile.pass);
      hasProfiles = true;
    }

    if (strcmp(ntpServer, values.ntpServer) != 0) {
      strlcpy(ntpServer, values.ntpServer, sizeof(ntpServer));
      sntpStarted = false;
    }

    // reconnect, so changed credentials take effect
    if (configApplied) WiFi.disconnect(false, false);
    configApplied = true;
    backoff.reset();
  }

  void connectIfDue() {
    if (!hasProfiles || !backoff.due(millis())) return;

    std::lock_guard<std::mutex> lock(radioMutex);
    if (wifiMulti.run(kConnectTimeoutMs) == WL_CONNECTED) {
      backoff.reset();
    } else {
      backoff.failed(millis());
    }
  }

  void startClockSync() {
    configTime(0, 0, ntpServer);
    sntpStarted = true;
  }

  void logConnectionChange(bool connected) {
    if (connected == wasConnected) return;
    wasConnected = connected;
    if (connected) {
      Serial.printf("wifi connected: %s (%d dBm), ip %s\n", WiFi.SSID().c_str(),
                    static_cast<int>(WiFi.RSSI()),
                    WiFi.localIP().toString().c_str());
    } else {
      Serial.println("wifi disconnected");
    }
  }

  void checkClock() {
    if (clockSynced) return;
    if (sntpHasSetTheClock && isPlausibleEpoch(time(nullptr))) {
      clockSynced = true;
      Serial.println("clock synced");
    }
  }
};

Uplink::Uplink(DeviceConfig& config)
    : impl_(new Impl(config)) {}

Uplink::~Uplink() = default;

void Uplink::begin() {
  sntp_set_time_sync_notification_cb(onSntpSync);
  WiFi.persistent(false);  // credentials live in our own config, not in the Wi-Fi NVS
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
}

void Uplink::update() {
  Impl& state = *impl_;

  const uint32_t version = state.config.version();
  if (!state.configApplied || version != state.appliedVersion) {
    state.appliedVersion = version;
    state.applyConfig();
  }

  const bool connected = WiFi.status() == WL_CONNECTED;
  state.logConnectionChange(connected);
  if (connected) {
    if (!state.sntpStarted) state.startClockSync();
    state.backoff.reset();
  } else {
    state.connectIfDue();
  }
  state.checkClock();
}

int Uplink::scanVisible(VisibleNetwork* out, size_t maxCount) {
  const ConfigValues values = impl_->config.snapshot();

  std::unique_lock<std::mutex> lock(impl_->radioMutex, std::try_to_lock);
  if (!lock.owns_lock()) return kScanBusy;

  const int found = WiFi.scanNetworks(false, true);
  if (found < 0) return kScanFailed;

  size_t count = 0;
  for (int i = 0; i < found; i++) {
    VisibleNetwork network;
    strlcpy(network.ssid, WiFi.SSID(i).c_str(), sizeof(network.ssid));
    network.rssi = static_cast<int8_t>(WiFi.RSSI(i));
    network.channel = static_cast<uint8_t>(WiFi.channel(i));
    network.security = securityName(WiFi.encryptionType(i));
    network.saved = isSaved(values, network.ssid);

    // insertion sort by signal, dropping the weakest once out is full
    size_t position = count;
    while (position > 0 && out[position - 1].rssi < network.rssi) position--;
    if (position >= maxCount) continue;
    if (count < maxCount) count++;
    for (size_t j = count - 1; j > position; j--) out[j] = out[j - 1];
    out[position] = network;
  }
  WiFi.scanDelete();
  return static_cast<int>(count);
}

bool Uplink::connected() const { return WiFi.status() == WL_CONNECTED; }

bool Uplink::clockSynced() const { return impl_->clockSynced; }

Uplink::Status Uplink::status() const {
  Status status;
  status.connected = WiFi.status() == WL_CONNECTED;
  if (status.connected) {
    strlcpy(status.ssid, WiFi.SSID().c_str(), sizeof(status.ssid));
    status.rssi = static_cast<int8_t>(WiFi.RSSI());
    strlcpy(status.ip, WiFi.localIP().toString().c_str(), sizeof(status.ip));
  }
  status.clockSynced = impl_->clockSynced;
  if (status.clockSynced) status.epoch = static_cast<uint32_t>(time(nullptr));
  return status;
}
