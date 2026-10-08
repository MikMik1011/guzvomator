#include "ParamCodec.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

uint8_t* fieldOf(ConfigValues& values, const ParamSpec& spec) {
  return reinterpret_cast<uint8_t*>(&values) + spec.offset;
}

const uint8_t* fieldOf(const ConfigValues& values, const ParamSpec& spec) {
  return reinterpret_cast<const uint8_t*>(&values) + spec.offset;
}

bool hasControlCharacter(const char* text) {
  for (const char* c = text; *c != '\0'; c++) {
    const unsigned char byte = *c;
    if (byte < 0x20 || byte == 0x7f) return true;
  }
  return false;
}

bool startsWith(const char* text, const char* prefix) {
  return strncmp(text, prefix, strlen(prefix)) == 0;
}

SetStatus checkIdentifier(const char* text) {
  if (*text == '\0') return SetStatus::InvalidFormat;
  for (const char* c = text; *c != '\0'; c++) {
    const bool allowed =
        isalnum(static_cast<unsigned char>(*c)) || *c == '-' || *c == '_';
    if (!allowed) return SetStatus::InvalidCharacters;
  }
  return SetStatus::Ok;
}

SetStatus checkHostname(const char* text) {
  if (*text == '\0') return SetStatus::InvalidFormat;
  for (const char* c = text; *c != '\0'; c++) {
    const bool allowed =
        isalnum(static_cast<unsigned char>(*c)) || *c == '-' || *c == '.';
    if (!allowed) return SetStatus::InvalidCharacters;
  }
  return SetStatus::Ok;
}

SetStatus checkUrl(const char* text) {
  if (*text == '\0') return SetStatus::Ok;
  const bool valid = startsWith(text, "http://") || startsWith(text, "https://");
  return valid ? SetStatus::Ok : SetStatus::InvalidFormat;
}

SetStatus checkTransport(const char* text) {
  const bool valid = strcmp(text, "http") == 0 || strcmp(text, "mqtt") == 0;
  return valid ? SetStatus::Ok : SetStatus::NotAllowed;
}

SetStatus checkTextRule(TextRule rule, const char* text) {
  switch (rule) {
    case TextRule::Any:
      return SetStatus::Ok;
    case TextRule::Identifier:
      return checkIdentifier(text);
    case TextRule::Hostname:
      return checkHostname(text);
    case TextRule::Url:
      return checkUrl(text);
    case TextRule::Transport:
      return checkTransport(text);
  }
  return SetStatus::InvalidFormat;
}

SetStatus parseInteger(const char* text, int32_t min, int32_t max,
                       int32_t& out) {
  if (*text == '\0') return SetStatus::NotANumber;

  errno = 0;
  char* end = nullptr;
  const long parsed = strtol(text, &end, 10);
  if (*end != '\0') return SetStatus::NotANumber;
  if (errno == ERANGE || parsed < min || parsed > max) {
    return SetStatus::OutOfRange;
  }
  out = static_cast<int32_t>(parsed);
  return SetStatus::Ok;
}

SetStatus applyTextValue(const ParamSpec& spec, ConfigValues& values,
                         const char* text) {
  const size_t length = strlen(text);
  if (length >= spec.size) return SetStatus::TooLong;
  if (hasControlCharacter(text)) return SetStatus::InvalidCharacters;

  const SetStatus rule = checkTextRule(spec.rule, text);
  if (rule != SetStatus::Ok) return rule;

  memcpy(fieldOf(values, spec), text, length + 1);
  return SetStatus::Ok;
}

SetStatus applyNumberValue(const ParamSpec& spec, ConfigValues& values,
                           const char* text) {
  int32_t parsed = 0;
  const SetStatus status = parseInteger(text, spec.min, spec.max, parsed);
  if (status != SetStatus::Ok) return status;

  if (spec.type == ParamType::UInt16) {
    const uint16_t value = static_cast<uint16_t>(parsed);
    memcpy(fieldOf(values, spec), &value, sizeof(value));
  } else {
    const int8_t value = static_cast<int8_t>(parsed);
    memcpy(fieldOf(values, spec), &value, sizeof(value));
  }
  return SetStatus::Ok;
}

}  // namespace

const char* describe(SetStatus status) {
  switch (status) {
    case SetStatus::Ok:
      return "ok";
    case SetStatus::UnknownParameter:
      return "unknown parameter";
    case SetStatus::TooLong:
      return "value too long";
    case SetStatus::NotANumber:
      return "not a number";
    case SetStatus::OutOfRange:
      return "out of range";
    case SetStatus::InvalidCharacters:
      return "invalid characters";
    case SetStatus::InvalidFormat:
      return "invalid format";
    case SetStatus::NotAllowed:
      return "value not allowed";
  }
  return "error";
}

SetStatus applyText(const ParamSpec& spec, ConfigValues& values,
                    const char* text) {
  if (spec.type == ParamType::Text) return applyTextValue(spec, values, text);
  return applyNumberValue(spec, values, text);
}

size_t formatValue(const ParamSpec& spec, const ConfigValues& values,
                   char* out, size_t cap) {
  const uint8_t* field = fieldOf(values, spec);

  switch (spec.type) {
    case ParamType::Text:
      return snprintf(out, cap, "%s", reinterpret_cast<const char*>(field));
    case ParamType::UInt16: {
      uint16_t value;
      memcpy(&value, field, sizeof(value));
      return snprintf(out, cap, "%u", static_cast<unsigned>(value));
    }
    case ParamType::Int8: {
      int8_t value;
      memcpy(&value, field, sizeof(value));
      return snprintf(out, cap, "%d", static_cast<int>(value));
    }
  }
  return 0;
}
