#include "Scanner.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_random.h>
#include <string.h>

#include <mutex>

#include "AddressHasher.h"
#include "WindowAggregator.h"

namespace {
constexpr uint32_t kScanIntervalMs = 100;
constexpr uint32_t kPollMs = 20;
}  // namespace

struct Scanner::Impl : public NimBLEScanCallbacks {
  uint8_t salt[AddressHasher::kMaxSaltLen];
  size_t saltLen = 0;
  WindowAggregator window;
  std::mutex mutex;

  void onResult(const NimBLEAdvertisedDevice* device) override {
    const NimBLEAddress address = device->getAddress();

    std::lock_guard<std::mutex> lock(mutex);
    const uint64_t deviceHash = AddressHasher::hash(
        salt, saltLen, address.getVal(), address.getType());
    window.add(deviceHash, device->getRSSI());
  }
};

Scanner::Scanner() : impl_(new Impl) {}
Scanner::~Scanner() = default;

bool Scanner::begin() {
  if (impl_->saltLen == 0) {
    impl_->saltLen = AddressHasher::kMaxSaltLen;
    esp_fill_random(impl_->salt, impl_->saltLen);
  }

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
  ScanResult result;
  result.windowS = windowS;
  if (windowS == 0) return result;

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->window.clear();
  }

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->start(static_cast<uint32_t>(windowS) * 1000, false);
  while (scan->isScanning()) delay(kPollMs);
  scan->stop();

  std::lock_guard<std::mutex> lock(impl_->mutex);
  result = impl_->window.summarize(rssiMin);
  result.windowS = windowS;
  impl_->window.clear();
  return result;
}

void Scanner::setSalt(const uint8_t* salt, size_t len) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->saltLen = len < AddressHasher::kMaxSaltLen
                       ? len
                       : AddressHasher::kMaxSaltLen;
  memcpy(impl_->salt, salt, impl_->saltLen);
}
