#include <Arduino.h>
#include <Cli.h>
#include <DeviceConfig.h>
#include <Scanner.h>

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kTaskStackBytes = 6144;
constexpr uint32_t kCliPollMs = 10;
constexpr uint8_t kLedOn = LOW;  // user LED on the XIAO ESP32-C6 is active-low
constexpr uint8_t kLedOff = HIGH;

DeviceConfig config;
Cli serialCli(config);
Scanner scanner;

void printResult(const ScanResult& result, int8_t rssiMin) {
  Serial.printf("window %us: devices %u, above %d dBm %u, avg rssi %d\n",
                result.windowS, result.uniqueDevices, rssiMin,
                result.uniqueDevicesAboveRssi, result.avgRssi);
}

void cliTask(void*) {
  for (;;) {
    serialCli.poll();
    delay(kCliPollMs);
  }
}

void scanTask(void*) {
  for (;;) {
    const ConfigValues values = config.snapshot();

    digitalWrite(LED_BUILTIN, kLedOn);
    const ScanResult result = scanner.scan(values.scanWindowS, values.rssiMin);
    digitalWrite(LED_BUILTIN, kLedOff);

    printResult(result, values.rssiMin);
    delay(static_cast<uint32_t>(values.scanPauseS) * 1000);
  }
}

void setup() {
  Serial.begin(kSerialBaud);
  Serial.setTxTimeoutMs(0);  // don't block when no monitor drains the USB buffer
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, kLedOff);

  config.begin();
  serialCli.begin();

  if (!scanner.begin()) {
    Serial.println("BLE init failed");
    while (true) delay(1000);
  }

  xTaskCreate(cliTask, "cli", kTaskStackBytes, nullptr, 1, nullptr);
  xTaskCreate(scanTask, "scan", kTaskStackBytes, nullptr, 1, nullptr);
}

void loop() { delay(1000); }
