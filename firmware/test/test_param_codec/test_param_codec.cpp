#include <string.h>
#include <unity.h>

#include "ParamCodec.h"

void setUp() {}
void tearDown() {}

namespace {

SetStatus apply(const char* name, ConfigValues& values, const char* text) {
  const ParamSpec* spec = findParam(name);
  TEST_ASSERT_NOT_NULL(spec);
  return applyText(*spec, values, text);
}

void expectFormatted(const char* name, const ConfigValues& values,
                     const char* expected) {
  char out[kMaxValueBuf];
  formatValue(*findParam(name), values, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING(expected, out);
}

}  // namespace

void test_find_param() {
  TEST_ASSERT_NOT_NULL(findParam("scan_window_s"));
  TEST_ASSERT_NULL(findParam("nope"));
  TEST_ASSERT_NULL(findParam(nullptr));
}

void test_every_param_fits_inside_config_values() {
  for (size_t i = 0; i < kParamCount; i++) {
    const ParamSpec& spec = kParams[i];
    const size_t size = spec.type == ParamType::Text    ? spec.size
                        : spec.type == ParamType::UInt16 ? 2
                                                         : 1;
    TEST_ASSERT_TRUE(spec.offset + size <= sizeof(ConfigValues));
    TEST_ASSERT_LESS_THAN(15, strlen(spec.name) + 1);  // NVS key limit
  }
}

void test_defaults() {
  const ConfigValues values;
  expectFormatted("transport", values, "http");
  expectFormatted("scan_window_s", values, "10");
  expectFormatted("scan_pause_s", values, "2");
  expectFormatted("rssi_min", values, "-80");
  expectFormatted("wifi0_ssid", values, "");
}

void test_number_accepts_values_inside_range() {
  ConfigValues values;
  TEST_ASSERT_EQUAL(SetStatus::Ok, apply("scan_window_s", values, "30"));
  TEST_ASSERT_EQUAL_UINT16(30, values.scanWindowS);
  TEST_ASSERT_EQUAL(SetStatus::Ok, apply("scan_pause_s", values, "0"));
  TEST_ASSERT_EQUAL_UINT16(0, values.scanPauseS);
  TEST_ASSERT_EQUAL(SetStatus::Ok, apply("rssi_min", values, "-90"));
  TEST_ASSERT_EQUAL_INT8(-90, values.rssiMin);
}

void test_number_rejects_bad_input() {
  ConfigValues values;
  TEST_ASSERT_EQUAL(SetStatus::OutOfRange, apply("scan_window_s", values, "0"));
  TEST_ASSERT_EQUAL(SetStatus::OutOfRange,
                    apply("scan_window_s", values, "3601"));
  TEST_ASSERT_EQUAL(SetStatus::OutOfRange,
                    apply("scan_window_s", values, "-1"));
  TEST_ASSERT_EQUAL(SetStatus::OutOfRange, apply("rssi_min", values, "-128"));
  TEST_ASSERT_EQUAL(SetStatus::OutOfRange, apply("rssi_min", values, "1"));
  TEST_ASSERT_EQUAL(SetStatus::OutOfRange,
                    apply("scan_window_s", values, "99999999999"));
  TEST_ASSERT_EQUAL(SetStatus::NotANumber, apply("scan_window_s", values, ""));
  TEST_ASSERT_EQUAL(SetStatus::NotANumber, apply("scan_window_s", values, "abc"));
  TEST_ASSERT_EQUAL(SetStatus::NotANumber, apply("scan_window_s", values, "10x"));
  TEST_ASSERT_EQUAL(SetStatus::NotANumber, apply("scan_window_s", values, "1.5"));
}

void test_failed_apply_leaves_value_unchanged() {
  ConfigValues values;
  apply("scan_window_s", values, "30");
  apply("scan_window_s", values, "banana");
  TEST_ASSERT_EQUAL_UINT16(30, values.scanWindowS);

  apply("transport", values, "mqtt");
  apply("transport", values, "ftp");
  TEST_ASSERT_EQUAL_STRING("mqtt", values.transport);
}

void test_text_length_boundary() {
  ConfigValues values;
  const char ssid32[] = "12345678901234567890123456789012";
  const char ssid33[] = "123456789012345678901234567890123";
  TEST_ASSERT_EQUAL(SetStatus::Ok, apply("wifi0_ssid", values, ssid32));
  TEST_ASSERT_EQUAL_STRING(ssid32, values.wifi[0].ssid);
  TEST_ASSERT_EQUAL(SetStatus::TooLong, apply("wifi0_ssid", values, ssid33));
  TEST_ASSERT_EQUAL_STRING(ssid32, values.wifi[0].ssid);
}

void test_empty_text_clears_the_value() {
  ConfigValues values;
  apply("wifi0_ssid", values, "home");
  TEST_ASSERT_EQUAL(SetStatus::Ok, apply("wifi0_ssid", values, ""));
  TEST_ASSERT_EQUAL_STRING("", values.wifi[0].ssid);
}

void test_wifi_slots_are_independent() {
  ConfigValues values;
  apply("wifi1_ssid", values, "office");
  apply("wifi1_pass", values, "secretpass");
  TEST_ASSERT_EQUAL_STRING("office", values.wifi[1].ssid);
  TEST_ASSERT_EQUAL_STRING("secretpass", values.wifi[1].pass);
  TEST_ASSERT_EQUAL_STRING("", values.wifi[0].ssid);
  TEST_ASSERT_EQUAL_STRING("", values.wifi[2].ssid);
}

void test_only_wifi_passwords_are_secret() {
  for (size_t i = 0; i < kParamCount; i++) {
    const bool isPassword = strstr(kParams[i].name, "_pass") != nullptr;
    TEST_ASSERT_EQUAL(isPassword, kParams[i].secret);
  }
}

void test_device_id_rules() {
  ConfigValues values;
  TEST_ASSERT_EQUAL(SetStatus::Ok, apply("device_id", values, "reading-room_1"));
  TEST_ASSERT_EQUAL(SetStatus::InvalidFormat, apply("device_id", values, ""));
  TEST_ASSERT_EQUAL(SetStatus::InvalidCharacters,
                    apply("device_id", values, "has space"));
  TEST_ASSERT_EQUAL(SetStatus::InvalidCharacters,
                    apply("device_id", values, "quote\""));
}

void test_transport_rules() {
  ConfigValues values;
  TEST_ASSERT_EQUAL(SetStatus::Ok, apply("transport", values, "mqtt"));
  TEST_ASSERT_EQUAL(SetStatus::Ok, apply("transport", values, "http"));
  TEST_ASSERT_EQUAL(SetStatus::NotAllowed, apply("transport", values, "ftp"));
  TEST_ASSERT_EQUAL(SetStatus::NotAllowed, apply("transport", values, ""));
}

void test_url_rules() {
  ConfigValues values;
  TEST_ASSERT_EQUAL(SetStatus::Ok,
                    apply("endpoint_url", values, "https://example.org/api"));
  TEST_ASSERT_EQUAL(SetStatus::Ok,
                    apply("endpoint_url", values, "http://10.0.0.5:8000/x"));
  TEST_ASSERT_EQUAL(SetStatus::Ok, apply("endpoint_url", values, ""));
  TEST_ASSERT_EQUAL(SetStatus::InvalidFormat,
                    apply("endpoint_url", values, "ftp://example.org"));
  TEST_ASSERT_EQUAL(SetStatus::InvalidFormat,
                    apply("endpoint_url", values, "example.org"));
}

void test_format_round_trip() {
  ConfigValues values;
  apply("scan_window_s", values, "45");
  apply("rssi_min", values, "-95");
  apply("mqtt_topic", values, "guzvomator/room");
  expectFormatted("scan_window_s", values, "45");
  expectFormatted("rssi_min", values, "-95");
  expectFormatted("mqtt_topic", values, "guzvomator/room");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_find_param);
  RUN_TEST(test_every_param_fits_inside_config_values);
  RUN_TEST(test_defaults);
  RUN_TEST(test_number_accepts_values_inside_range);
  RUN_TEST(test_number_rejects_bad_input);
  RUN_TEST(test_failed_apply_leaves_value_unchanged);
  RUN_TEST(test_text_length_boundary);
  RUN_TEST(test_empty_text_clears_the_value);
  RUN_TEST(test_wifi_slots_are_independent);
  RUN_TEST(test_only_wifi_passwords_are_secret);
  RUN_TEST(test_device_id_rules);
  RUN_TEST(test_transport_rules);
  RUN_TEST(test_url_rules);
  RUN_TEST(test_format_round_trip);
  return UNITY_END();
}
