#pragma once
#include <math.h>

// Environmental readings; NAN means "not provided by this sensor".
struct EnvReading {
  float temperatureC = NAN;
  float humidityPct = NAN;
  float illuminanceLux = NAN;
};

class ISensor {
 public:
  virtual ~ISensor() = default;
  virtual bool begin() = 0;
  // Fills only the fields this sensor provides.
  virtual bool read(EnvReading& out) = 0;
};
