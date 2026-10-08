#pragma once
#include <stddef.h>

#include <DeviceConfig.h>

#include <memory>

#include "LineBuffer.h"

namespace CmdProc {
class Proc;
}

class Uplink;
class Reporter;

class Cli {
 public:
  Cli(DeviceConfig& config, Uplink& uplink, const Reporter& reporter);
  ~Cli();

  void begin();
  void poll();  // non-blocking

 private:
  void handleLine();

  DeviceConfig& config_;
  Uplink& uplink_;
  const Reporter& reporter_;
  std::unique_ptr<CmdProc::Proc> proc_;
  LineBuffer line_;
};
