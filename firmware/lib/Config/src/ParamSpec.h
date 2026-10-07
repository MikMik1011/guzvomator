#pragma once
#include <stddef.h>
#include <stdint.h>

#include "Config.h"

enum class ParamType : uint8_t { Text, UInt16, Int8 };
enum class TextRule : uint8_t { Any, Identifier, Url, Transport };

struct ParamSpec {
  const char* name;
  ParamType type;
  size_t offset;  // of the field inside ConfigValues
  size_t size;    // Text only: buffer size including the terminator
  int32_t min;    // numeric only
  int32_t max;
  TextRule rule;
  bool secret;
};

constexpr size_t kMaxValueBuf = 160;

extern const ParamSpec kParams[];
extern const size_t kParamCount;

const ParamSpec* findParam(const char* name);
