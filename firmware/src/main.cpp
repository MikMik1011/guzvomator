#include <Arduino.h>
#include <Cli.h>
#include <DeviceConfig.h>
#include <HttpTransport.h>
#include <MqttTransport.h>
#include <Reporter.h>
#include <Scanner.h>
#include <Uplink.h>
#include <string.h>
#include <time.h>

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kTaskStackBytes = 6144;
constexpr uint32_t kScanStackBytes = 12288;  // a TLS handshake needs a deep stack
constexpr uint32_t kNetworkStackBytes = 8192;
constexpr uint32_t kCliPollMs = 10;
constexpr uint32_t kNetworkPollMs = 1000;
constexpr uint32_t kClockWaitMs = 500;
constexpr uint8_t kLedOn = LOW;  // user LED on the XIAO ESP32-C6 is active-low
constexpr uint8_t kLedOff = HIGH;

DeviceConfig config;
Uplink uplink(config);
Reporter reporter;
Cli serialCli(config, uplink, reporter);
Scanner scanner;

void printResult(const ScanResult& result, int8_t rssiMin) {
  Serial.printf("window %us: devices %u, above %d dBm %u, avg rssi %d\n",
                result.windowS, result.uniqueDevices, rssiMin,
                result.uniqueDevicesAboveRssi, result.avgRssi);
}

Reading makeReading(const ConfigValues& values, const ScanResult& result) {
  Reading reading;
  strlcpy(reading.deviceId, values.deviceId, sizeof(reading.deviceId));
  reading.ts = static_cast<uint32_t>(time(nullptr));
  reading.rssiMin = values.rssiMin;
  reading.scan = result;
  return reading;
}

void logFlush(const ITransport& transport,
              const Reporter::FlushSummary& summary) {
  char reason[48];
  transport.describe(summary.failure, reason, sizeof(reason));

  if (summary.sent > 0) {
    Serial.printf("sent %u reading(s), %u queued\n",
                  static_cast<unsigned>(summary.sent),
                  static_cast<unsigned>(reporter.queued()));
  }
  if (summary.rejected > 0) {
    Serial.printf("backend rejected %u reading(s): %s\n",
                  static_cast<unsigned>(summary.rejected), reason);
  }
  if (summary.stalled) {
    Serial.printf("send failed: %s, %u queued\n", reason,
                  static_cast<unsigned>(reporter.queued()));
  }
}

void deliverQueued(const ConfigValues& values) {
  if (!uplink.connected()) return;

  if (strcmp(values.transport, "mqtt") == 0) {
    MqttTransport transport({values.mqttHost, values.mqttPort, values.mqttUser,
                             values.mqttPass, values.deviceId,
                             values.mqttTopic});
    logFlush(transport, reporter.flush(transport, millis()));
  } else {
    HttpTransport transport(values.endpointUrl, values.apiKey);
    logFlush(transport, reporter.flush(transport, millis()));
  }
}

void cliTask(void*) {
  for (;;) {
    serialCli.poll();
    delay(kCliPollMs);
  }
}

void networkTask(void*) {
  for (;;) {
    uplink.update();
    delay(kNetworkPollMs);
  }
}

void scanTask(void*) {
  for (;;) {
    if (!uplink.clockSynced()) {
      delay(kClockWaitMs);
      continue;
    }

    const ConfigValues values = config.snapshot();

    digitalWrite(LED_BUILTIN, kLedOn);
    const ScanResult result = scanner.scan(values.scanWindowS, values.rssiMin);
    digitalWrite(LED_BUILTIN, kLedOff);

    printResult(result, values.rssiMin);
    reporter.submit(makeReading(values, result));
    deliverQueued(values);
    delay(static_cast<uint32_t>(values.scanPauseS) * 1000);
  }
}

void setup() {
  Serial.begin(kSerialBaud);
  Serial.setTxTimeoutMs(0);  // don't block when no monitor drains the USB buffer
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, kLedOff);

  config.begin();
  uplink.begin();
  serialCli.begin();

  if (!scanner.begin()) {
    Serial.println("BLE init failed");
    while (true) delay(1000);
  }

  xTaskCreate(cliTask, "cli", kTaskStackBytes, nullptr, 1, nullptr);
  xTaskCreate(networkTask, "uplink", kNetworkStackBytes, nullptr, 1, nullptr);
  xTaskCreate(scanTask, "scan", kScanStackBytes, nullptr, 1, nullptr);
}

void loop() { delay(1000); }
