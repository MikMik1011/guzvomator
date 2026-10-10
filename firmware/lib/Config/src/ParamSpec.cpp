#include "ParamSpec.h"

#include <string.h>

namespace {

constexpr ParamSpec text(const char* name, size_t offset, size_t size,
                         TextRule rule, bool secret = false) {
  return {name, ParamType::Text, offset, size, 0, 0, rule, secret};
}

constexpr ParamSpec number(const char* name, ParamType type, size_t offset,
                           int32_t min, int32_t max) {
  return {name, type, offset, 0, min, max, TextRule::Any, false};
}

constexpr size_t wifiOffset(int slot, size_t fieldOffset) {
  return offsetof(ConfigValues, wifi) + slot * sizeof(WifiProfile) +
         fieldOffset;
}

constexpr ParamSpec wifiSsid(const char* name, int slot) {
  return text(name, wifiOffset(slot, offsetof(WifiProfile, ssid)),
              sizeof(WifiProfile::ssid), TextRule::Any);
}

constexpr ParamSpec wifiPass(const char* name, int slot) {
  return text(name, wifiOffset(slot, offsetof(WifiProfile, pass)),
              sizeof(WifiProfile::pass), TextRule::Any, true);
}

}  // namespace

const ParamSpec kParams[] = {
    text("device_id", offsetof(ConfigValues, deviceId),
         sizeof(ConfigValues::deviceId), TextRule::Identifier),
    wifiSsid("wifi0_ssid", 0),
    wifiPass("wifi0_pass", 0),
    wifiSsid("wifi1_ssid", 1),
    wifiPass("wifi1_pass", 1),
    wifiSsid("wifi2_ssid", 2),
    wifiPass("wifi2_pass", 2),
    text("transport", offsetof(ConfigValues, transport),
         sizeof(ConfigValues::transport), TextRule::Transport),
    text("endpoint_url", offsetof(ConfigValues, endpointUrl),
         sizeof(ConfigValues::endpointUrl), TextRule::Url),
    text("api_key", offsetof(ConfigValues, apiKey),
         sizeof(ConfigValues::apiKey), TextRule::Any, true),
    text("mqtt_host", offsetof(ConfigValues, mqttHost),
         sizeof(ConfigValues::mqttHost), TextRule::Any),
    number("mqtt_port", ParamType::UInt16, offsetof(ConfigValues, mqttPort), 1,
           65535),
    text("mqtt_user", offsetof(ConfigValues, mqttUser),
         sizeof(ConfigValues::mqttUser), TextRule::Any),
    text("mqtt_pass", offsetof(ConfigValues, mqttPass),
         sizeof(ConfigValues::mqttPass), TextRule::Any, true),
    text("mqtt_topic", offsetof(ConfigValues, mqttTopic),
         sizeof(ConfigValues::mqttTopic), TextRule::Any),
    text("ntp_server", offsetof(ConfigValues, ntpServer),
         sizeof(ConfigValues::ntpServer), TextRule::Hostname),
    number("scan_window_s", ParamType::UInt16,
           offsetof(ConfigValues, scanWindowS), 1, 3600),
    number("scan_pause_s", ParamType::UInt16,
           offsetof(ConfigValues, scanPauseS), 0, 3600),
    number("rssi_min", ParamType::Int8, offsetof(ConfigValues, rssiMin), -127,
           0),
};

const size_t kParamCount = sizeof(kParams) / sizeof(kParams[0]);

const ParamSpec* findParam(const char* name) {
  if (name == nullptr) return nullptr;
  for (size_t i = 0; i < kParamCount; i++) {
    if (strcmp(kParams[i].name, name) == 0) return &kParams[i];
  }
  return nullptr;
}
