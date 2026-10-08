#pragma once
#include <stddef.h>

#include "Reading.h"

constexpr int kPayloadVersion = 1;
constexpr size_t kMaxPayloadLen = 256;

// Writes the JSON for one reading and returns its length, excluding the NUL.
// Returns 0 if buf is too small or the device id is not a plain identifier.
size_t buildPayload(const Reading& reading, char* buf, size_t cap);
