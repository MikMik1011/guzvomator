#pragma once
#include <stddef.h>

#include "ParamSpec.h"

enum class SetStatus : uint8_t {
  Ok,
  UnknownParameter,
  TooLong,
  NotANumber,
  OutOfRange,
  InvalidCharacters,
  InvalidFormat,
  NotAllowed,
};

const char* describe(SetStatus status);

// Validates text and stores it in values. On failure values is unchanged.
SetStatus applyText(const ParamSpec& spec, ConfigValues& values,
                    const char* text);

// Writes the raw value as text (secrets included) and returns its length.
size_t formatValue(const ParamSpec& spec, const ConfigValues& values,
                   char* out, size_t cap);
