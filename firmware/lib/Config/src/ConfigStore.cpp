#include "ConfigStore.h"

#include <Preferences.h>
#include <string.h>

#include "ParamCodec.h"

namespace {
constexpr const char* kNamespace = "guzvo";
}  // namespace

bool ConfigStore::load(ConfigValues& values) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;

  char stored[kMaxValueBuf];
  for (size_t i = 0; i < kParamCount; i++) {
    if (!prefs.isKey(kParams[i].name)) continue;
    prefs.getString(kParams[i].name, stored, sizeof(stored));
    applyText(kParams[i], values, stored);
  }
  prefs.end();
  return true;
}

bool ConfigStore::save(const ConfigValues& values) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;

  bool ok = true;
  char current[kMaxValueBuf];
  char stored[kMaxValueBuf];
  for (size_t i = 0; i < kParamCount; i++) {
    const ParamSpec& spec = kParams[i];
    formatValue(spec, values, current, sizeof(current));

    stored[0] = '\0';
    bool unchanged = false;
    if (prefs.isKey(spec.name)) {
      prefs.getString(spec.name, stored, sizeof(stored));
      unchanged = strcmp(stored, current) == 0;
    }
    if (unchanged) continue;

    // putString returns 0 both on failure and for an empty string
    const bool written =
        prefs.putString(spec.name, current) > 0 || current[0] == '\0';
    ok = ok && written;
  }
  prefs.end();
  return ok;
}

bool ConfigStore::erase() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  const bool ok = prefs.clear();
  prefs.end();
  return ok;
}
