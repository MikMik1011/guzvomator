#pragma once
#include <stdint.h>

enum class TransportKind : uint8_t { Http, Mqtt };

struct Config {
  char deviceId[32] = "";
  char wifiSsid[33] = "";
  char wifiPass[65] = "";
  TransportKind transport = TransportKind::Http;
  char endpointUrl[128] = "";
  char mqttHost[64] = "";
  char mqttTopic[64] = "";
  uint16_t scanWindowS = 30;
  uint32_t sleepIntervalS = 300;
  int8_t rssiMin = -80;
};
