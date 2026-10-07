#pragma once
#include <stddef.h>

#include <DeviceConfig.h>

#include <memory>

namespace CmdProc {
class Proc;
}

class Cli {
 public:
  explicit Cli(DeviceConfig& config);
  ~Cli();

  void begin();
  void poll();  // non-blocking

 private:
  static constexpr size_t kLineCapacity = 192;

  void handleLine();

  DeviceConfig& config_;
  std::unique_ptr<CmdProc::Proc> proc_;
  char line_[kLineCapacity];
  size_t length_ = 0;
  bool overflowed_ = false;
};
