#pragma once
#include <stddef.h>
#include <stdint.h>

#include <DeviceConfig.h>

#include <memory>

// Keeps Wi-Fi connected to one of the saved profiles and the clock synced.
// update() must be called about once a second from a single task. It can block
// for several seconds while it scans and connects.
class Uplink {
 public:
  struct Status {
    bool connected = false;
    char ssid[33] = "";
    int8_t rssi = 0;
    char ip[16] = "";
    bool clockSynced = false;
    uint32_t epoch = 0;  // UTC seconds, valid when clockSynced
  };

  struct VisibleNetwork {
    char ssid[33] = "";
    int8_t rssi = 0;
    uint8_t channel = 0;
    const char* security = "";
    bool saved = false;
  };

  explicit Uplink(DeviceConfig& config);
  ~Uplink();

  void begin();
  void update();

  // Scans for networks, strongest first, and returns how many were written to
  // out. Blocks for a few seconds. Returns kScanBusy if the uplink task is
  // connecting, or kScanFailed if the scan itself failed.
  static constexpr int kScanBusy = -1;
  static constexpr int kScanFailed = -2;
  int scanVisible(VisibleNetwork* out, size_t maxCount);

  // True once SNTP has set the clock this boot, and stays true if Wi-Fi drops.
  bool clockSynced() const;

  Status status() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
