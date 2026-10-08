#include "Cli.h"

#include <Arduino.h>
#include <Uplink.h>
#include <cmdProc.h>
#include <time.h>

#include <ParamSpec.h>

namespace {

constexpr const char* kMaskedSecret = "****";

// CmdProc callbacks are plain function pointers, so they reach the config and
// report errors through these.
DeviceConfig* config = nullptr;
Uplink* uplink = nullptr;
const char* lastError = nullptr;

int fail(const char* message) {
  lastError = message;
  return CMDPROC_ERR_INVALIDVALUE;
}

bool isPlainText(const char* text) {
  const size_t length = strlen(text);
  if (length > 0 && (text[0] == ' ' || text[length - 1] == ' ')) return false;
  for (const char* c = text; *c != '\0'; c++) {
    const unsigned char byte = *c;
    if (byte < 0x20 || byte > 0x7e || byte == '"' || byte == '\\') return false;
  }
  return true;
}

// Plain text is printed as is. Anything with hidden characters is quoted and
// escaped, so a stray space or look-alike character shows up.
void printText(const char* text) {
  if (isPlainText(text)) {
    Serial.print(text);
    return;
  }
  Serial.print('"');
  for (const char* c = text; *c != '\0'; c++) {
    const unsigned char byte = *c;
    if (byte >= 0x20 && byte <= 0x7e && byte != '"' && byte != '\\') {
      Serial.write(byte);
    } else {
      Serial.printf("\\x%02x", byte);
    }
  }
  Serial.print('"');
}

void printParam(const ParamSpec& spec, const char* value) {
  const bool masked = spec.secret && value[0] != '\0';
  Serial.printf("%s=", spec.name);
  if (masked) {
    Serial.print(kMaskedSecret);
  } else {
    printText(value);
  }
  Serial.println();
}

int cmdList(CmdProc::Proc&) {
  char value[kMaxValueBuf];
  for (size_t i = 0; i < kParamCount; i++) {
    config->get(kParams[i].name, value, sizeof(value));
    printParam(kParams[i], value);
  }
  return 0;
}

int cmdGet(CmdProc::Proc& proc) {
  const char* name = proc.GetNextToken();
  const ParamSpec* spec = findParam(name);
  if (spec == nullptr) return fail(describe(SetStatus::UnknownParameter));

  char value[kMaxValueBuf];
  config->get(name, value, sizeof(value));
  printParam(*spec, value);
  return 0;
}

int cmdSet(CmdProc::Proc& proc) {
  const char* name = proc.GetNextToken();
  const char* value = proc.GetNextToken();

  const SetStatus status = config->set(name, value != nullptr ? value : "");
  if (status != SetStatus::Ok) return fail(describe(status));

  Serial.println("unsaved changes, run 'save' to keep them");
  return 0;
}

int cmdSave(CmdProc::Proc&) {
  return config->save() ? 0 : fail("could not write storage");
}

int cmdReset(CmdProc::Proc&) {
  config->reset();
  return 0;
}

void printNetworkStatus() {
  const Uplink::Status status = uplink->status();

  if (status.connected) {
    Serial.printf("wifi=connected\nwifi_ssid=%s\nwifi_rssi=%d\nip=%s\n",
                  status.ssid, static_cast<int>(status.rssi), status.ip);
  } else {
    Serial.println("wifi=disconnected");
  }

  Serial.printf("clock=%s\n", status.clockSynced ? "synced" : "not synced");
  if (!status.clockSynced) return;

  const time_t epoch = status.epoch;
  struct tm utc;
  gmtime_r(&epoch, &utc);
  char text[24];
  strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", &utc);
  Serial.printf("time_utc=%s\n", text);
}

int cmdWifiScan(CmdProc::Proc&) {
  constexpr size_t kMaxListed = 24;
  Uplink::VisibleNetwork networks[kMaxListed];

  Serial.println("scanning, this takes a few seconds");
  const int count = uplink->scanVisible(networks, kMaxListed);
  if (count == Uplink::kScanBusy) return fail("busy connecting, try again");
  if (count == Uplink::kScanFailed) return fail("scan failed");

  for (int i = 0; i < count; i++) {
    const Uplink::VisibleNetwork& network = networks[i];
    Serial.printf("%4d dBm  ch %-2u  %-10s ", static_cast<int>(network.rssi),
                  static_cast<unsigned>(network.channel), network.security);
    if (network.ssid[0] != '\0') {
      printText(network.ssid);
    } else {
      Serial.print("(hidden)");
    }
    Serial.println(network.saved ? "  [saved]" : "");
  }
  Serial.printf("networks=%d\n", count);
  return 0;
}

int cmdStatus(CmdProc::Proc&) {
  printNetworkStatus();
  Serial.printf("unsaved=%s\n", config->hasUnsavedChanges() ? "yes" : "no");
  Serial.printf("config_version=%lu\n",
                static_cast<unsigned long>(config->version()));
  Serial.printf("uptime_s=%lu\n", static_cast<unsigned long>(millis() / 1000));
  Serial.printf("free_heap=%lu\n", static_cast<unsigned long>(ESP.getFreeHeap()));
  return 0;
}

int cmdReboot(CmdProc::Proc&) {
  if (config->hasUnsavedChanges()) Serial.println("unsaved changes discarded");
  Serial.println("OK");
  Serial.flush();
  ESP.restart();
  return 0;
}

int cmdHelp(CmdProc::Proc&) {
  Serial.println("list                  show all parameters");
  Serial.println("get <name>            show one parameter");
  Serial.println("set <name> [value]    change a parameter, no value clears it");
  Serial.println("save                  write changes to flash");
  Serial.println("reset                 restore defaults and erase flash");
  Serial.println("status                show runtime state");
  Serial.println("wifi_scan             list visible Wi-Fi networks");
  Serial.println("reboot                restart the device");
  return 0;
}

const char* describeError(int code) {
  switch (code) {
    case CMDPROC_ERR_UNKNOWNCOMMAND:
      return "unknown command, try help";
    case CMDPROC_ERR_MISSINGARGUMENT:
      return "missing argument";
    case CMDPROC_ERR_EXTRAARGUMENT:
      return "too many arguments";
    default:
      return "failed";
  }
}

}  // namespace

Cli::Cli(DeviceConfig& deviceConfig, Uplink& uplinkRef)
    : config_(deviceConfig),
      uplink_(uplinkRef),
      proc_(new CmdProc::Proc) {}

Cli::~Cli() = default;

void Cli::begin() {
  config = &config_;
  uplink = &uplink_;

  proc_->Init(9);
  proc_->Add("list", cmdList, 1, 1);
  proc_->Add("get", cmdGet, 2, 2);
  proc_->Add("set", cmdSet, 2, 3);
  proc_->Add("save", cmdSave, 1, 1);
  proc_->Add("reset", cmdReset, 1, 1);
  proc_->Add("status", cmdStatus, 1, 1);
  proc_->Add("reboot", cmdReboot, 1, 1);
  proc_->Add("wifi_scan", cmdWifiScan, 1, 1);
  proc_->Add("help", cmdHelp, 1, 1);
}

void Cli::poll() {
  while (Serial.available()) {
    switch (line_.feed(Serial.read())) {
      case LineBuffer::Event::LineReady:
        handleLine();
        break;
      case LineBuffer::Event::TooLong:
        Serial.println("ERR line too long");
        break;
      case LineBuffer::Event::None:
        break;
    }
  }
}

void Cli::handleLine() {
  lastError = nullptr;
  const int code = proc_->Parse(line_.text());

  if (code == 0) {
    Serial.println("OK");
  } else {
    Serial.printf("ERR %s\n", lastError != nullptr ? lastError : describeError(code));
  }
}
