#pragma once
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "ScanResult.h"

class Scanner {
 public:
  Scanner();
  ~Scanner();

  bool begin();
  // Blocks for windowS seconds. Hashes live in RAM only for this call.
  ScanResult scan(uint16_t windowS, int8_t rssiMin);
  void setSalt(const uint8_t* salt, size_t len);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
