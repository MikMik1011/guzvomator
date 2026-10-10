#pragma once
#include <stdint.h>

constexpr int kWifiProfileCount = 3;

struct WifiProfile {
  char ssid[33] = "";
  char pass[64] = "";
};

struct ConfigValues {
  char deviceId[32] = "";
  WifiProfile wifi[kWifiProfileCount];
  char transport[8] = "http";
  char endpointUrl[128] = "";
  char apiKey[65] = "";
  char mqttHost[64] = "";
  uint16_t mqttPort = 1883;
  char mqttUser[33] = "";
  char mqttPass[65] = "";
  char mqttTopic[64] = "guzvomator/readings";
  char ntpServer[64] = "pool.ntp.org";
  uint16_t scanWindowS = 10;
  uint16_t scanPauseS = 2;
  int8_t rssiMin = -80;
};
