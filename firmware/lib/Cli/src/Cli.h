#pragma once
#include <stddef.h>

#include <DeviceConfig.h>

#include <memory>

#include "LineBuffer.h"

namespace CmdProc {
class Proc;
}

class Uplink;

class Cli {
 public:
  Cli(DeviceConfig& config, Uplink& uplink);
  ~Cli();

  void begin();
  void poll();  // non-blocking

 private:
  void handleLine();

  DeviceConfig& config_;
  Uplink& uplink_;
  std::unique_ptr<CmdProc::Proc> proc_;
  LineBuffer line_;
};
