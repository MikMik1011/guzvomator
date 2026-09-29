#include <Arduino.h>

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kBlinkIntervalMs = 500;
constexpr uint8_t kLedOn = LOW;  // user LED on the XIAO ESP32-C6 is active-low
constexpr uint8_t kLedOff = HIGH;

void setup() {
  Serial.begin(kSerialBaud);
  Serial.setTxTimeoutMs(0);  // don't block when no monitor drains the USB buffer
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, kLedOff);
  Serial.println("Guzvomator: hello world");
}

void loop() {
  static bool ledIsOn = false;
  ledIsOn = !ledIsOn;
  digitalWrite(LED_BUILTIN, ledIsOn ? kLedOn : kLedOff);
  Serial.printf("uptime %lu ms, led %s\n", millis(), ledIsOn ? "on" : "off");
  delay(kBlinkIntervalMs);
}
