#include "Scanner.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_random.h>
#include <mbedtls/platform_util.h>

#include <mutex>

#include "AddressHasher.h"
#include "WindowAggregator.h"

namespace {
constexpr uint32_t kScanIntervalMs = 100;
constexpr uint32_t kPollMs = 20;
}  // namespace

struct Scanner::Impl : public NimBLEScanCallbacks {
  uint8_t salt[AddressHasher::kMaxSaltLen];
  WindowAggregator window;
  std::mutex mutex;

  void onResult(const NimBLEAdvertisedDevice* device) override {
    const NimBLEAddress address = device->getAddress();

    std::lock_guard<std::mutex> lock(mutex);
    const uint64_t deviceHash = AddressHasher::hash(
        salt, sizeof(salt), address.getVal(), address.getType());
    window.add(deviceHash, device->getRSSI());
  }

  void startWindow() {
    std::lock_guard<std::mutex> lock(mutex);
    window.clear();
    esp_fill_random(salt, sizeof(salt));
  }

  ScanResult endWindow(int8_t rssiMin) {
    std::lock_guard<std::mutex> lock(mutex);
    const ScanResult result = window.summarize(rssiMin);
    window.clear();
    mbedtls_platform_zeroize(salt, sizeof(salt));
    return result;
  }
};

Scanner::Scanner() : impl_(new Impl) {}
Scanner::~Scanner() = default;

bool Scanner::begin() {
  if (!NimBLEDevice::init("")) return false;

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(impl_.get(), true);
  scan->setActiveScan(false);
  scan->setInterval(kScanIntervalMs);
  scan->setWindow(kScanIntervalMs);
  scan->setMaxResults(0);
  return true;
}

ScanResult Scanner::scan(uint16_t windowS, int8_t rssiMin) {
  if (windowS == 0) return ScanResult();

  impl_->startWindow();

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->start(static_cast<uint32_t>(windowS) * 1000, false);
  while (scan->isScanning()) delay(kPollMs);
  scan->stop();

  ScanResult result = impl_->endWindow(rssiMin);
  result.windowS = windowS;
  return result;
}
