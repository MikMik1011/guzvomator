#include "Cli.h"

#include <Arduino.h>
#include <cmdProc.h>

#include <ParamSpec.h>

namespace {

constexpr const char* kMaskedSecret = "****";

// CmdProc callbacks are plain function pointers, so they reach the config and
// report errors through these.
DeviceConfig* config = nullptr;
const char* lastError = nullptr;

int fail(const char* message) {
  lastError = message;
  return CMDPROC_ERR_INVALIDVALUE;
}

void printParam(const ParamSpec& spec, const char* value) {
  const bool masked = spec.secret && value[0] != '\0';
  Serial.printf("%s=%s\n", spec.name, masked ? kMaskedSecret : value);
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

int cmdStatus(CmdProc::Proc&) {
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

Cli::Cli(DeviceConfig& deviceConfig)
    : config_(deviceConfig), proc_(new CmdProc::Proc) {}

Cli::~Cli() = default;

void Cli::begin() {
  config = &config_;

  proc_->Init(8);
  proc_->Add("list", cmdList, 1, 1);
  proc_->Add("get", cmdGet, 2, 2);
  proc_->Add("set", cmdSet, 2, 3);
  proc_->Add("save", cmdSave, 1, 1);
  proc_->Add("reset", cmdReset, 1, 1);
  proc_->Add("status", cmdStatus, 1, 1);
  proc_->Add("reboot", cmdReboot, 1, 1);
  proc_->Add("help", cmdHelp, 1, 1);
}

void Cli::poll() {
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\r') continue;

    if (c != '\n') {
      if (length_ < kLineCapacity - 1) {
        line_[length_++] = c;
      } else {
        overflowed_ = true;
      }
      continue;
    }

    if (overflowed_) {
      Serial.println("ERR line too long");
    } else if (length_ > 0) {
      line_[length_] = '\0';
      handleLine();
    }
    length_ = 0;
    overflowed_ = false;
  }
}

void Cli::handleLine() {
  lastError = nullptr;
  const int code = proc_->Parse(line_);

  if (code == 0) {
    Serial.println("OK");
  } else {
    Serial.printf("ERR %s\n", lastError != nullptr ? lastError : describeError(code));
  }
}
