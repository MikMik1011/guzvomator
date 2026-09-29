#pragma once
#include <stddef.h>

class ITransport {
 public:
  virtual ~ITransport() = default;
  virtual bool begin() = 0;
  virtual bool send(const char* json, size_t len) = 0;
};
