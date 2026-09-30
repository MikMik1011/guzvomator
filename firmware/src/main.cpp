#include <Arduino.h>
#include <Scanner.h>

constexpr uint32_t kSerialBaud = 115200;
constexpr uint16_t kScanWindowS = 10;
constexpr int8_t kRssiMin = -80;
constexpr uint32_t kPauseBetweenScansMs = 2000;
constexpr uint8_t kLedOn = LOW;  // user LED on the XIAO ESP32-C6 is active-low
constexpr uint8_t kLedOff = HIGH;

Scanner scanner;

void printResult(const ScanResult& result) {
  Serial.printf("window %us: devices %u, above %d dBm %u, avg rssi %d |",
                result.windowS, result.uniqueDevices, kRssiMin,
                result.uniqueDevicesAboveRssi, result.avgRssi);
  for (size_t i = 0; i < kSweepSize; i++) {
    Serial.printf(" %d:%u", kSweepRssi[i], result.sweepCounts[i]);
  }
  Serial.println();
}

void setup() {
  Serial.begin(kSerialBaud);
  Serial.setTxTimeoutMs(0);  // don't block when no monitor drains the USB buffer
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, kLedOff);

  if (!scanner.begin()) {
    Serial.println("BLE init failed");
    while (true) delay(1000);
  }
}

void loop() {
  digitalWrite(LED_BUILTIN, kLedOn);
  const ScanResult result = scanner.scan(kScanWindowS, kRssiMin);
  digitalWrite(LED_BUILTIN, kLedOff);

  printResult(result);
  delay(kPauseBetweenScansMs);
}
