#pragma once
#include <stddef.h>
#include <stdint.h>

#include "ISensor.h"
#include "Scanner.h"

// Returns bytes written (excluding NUL), or 0 if buf is too small.
size_t buildPayload(char* buf, size_t cap, const char* deviceId,
                    uint32_t timestamp, const ScanResult& scan,
                    const EnvReading& env);
