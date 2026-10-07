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
  // Blocks for windowS seconds. Uses a fresh random salt that is wiped, with
  // the hashes, before returning.
  ScanResult scan(uint16_t windowS, int8_t rssiMin);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
