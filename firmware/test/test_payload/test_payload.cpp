#include <string.h>
#include <unity.h>

#include <initializer_list>

#include "Payload.h"

void setUp() {}
void tearDown() {}

namespace {

Reading sampleReading() {
  Reading reading;
  strcpy(reading.deviceId, "guzvo-a1b2c3");
  reading.ts = 1790000000;
  reading.rssiMin = -80;
  reading.scan.windowS = 10;
  reading.scan.uniqueDevices = 12;
  reading.scan.uniqueDevicesAboveRssi = 5;
  reading.scan.avgRssi = -83;
  return reading;
}

const char kWithoutSensors[] =
    "{\"v\":1,\"device_id\":\"guzvo-a1b2c3\",\"ts\":1790000000,"
    "\"scan_window_s\":10,\"rssi_min\":-80,\"devices\":12,"
    "\"devices_above_rssi\":5,\"avg_rssi\":-83}";

}  // namespace

void test_reading_without_sensors() {
  char buf[kMaxPayloadLen];
  const size_t length = buildPayload(sampleReading(), buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING(kWithoutSensors, buf);
  TEST_ASSERT_EQUAL(strlen(kWithoutSensors), length);
}

void test_reading_with_all_sensors() {
  Reading reading = sampleReading();
  reading.env.temperatureC = 22.44f;
  reading.env.humidityPct = 41.0f;
  reading.env.illuminanceLux = 312.0f;

  char buf[kMaxPayloadLen];
  buildPayload(reading, buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(
      buf, "\"avg_rssi\":-83,\"temp_c\":22.4,\"humidity_pct\":41.0,\"lux\":312.0}"));
}

void test_missing_sensor_values_are_omitted_one_by_one() {
  Reading reading = sampleReading();
  reading.env.illuminanceLux = 5.0f;

  char buf[kMaxPayloadLen];
  buildPayload(reading, buf, sizeof(buf));
  TEST_ASSERT_NULL(strstr(buf, "temp_c"));
  TEST_ASSERT_NULL(strstr(buf, "humidity_pct"));
  TEST_ASSERT_NOT_NULL(strstr(buf, ",\"lux\":5.0}"));
}

void test_negative_sensor_values() {
  Reading reading = sampleReading();
  reading.env.temperatureC = -3.25f;

  char buf[kMaxPayloadLen];
  buildPayload(reading, buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp_c\":-3.2}"));
}

void test_largest_reading_fits_the_maximum_length() {
  Reading reading;
  memset(reading.deviceId, 'd', kDeviceIdCapacity - 1);
  reading.ts = 4294967295u;
  reading.rssiMin = -127;
  reading.scan.windowS = 3600;
  reading.scan.uniqueDevices = 65535;
  reading.scan.uniqueDevicesAboveRssi = 65535;
  reading.scan.avgRssi = -128;
  reading.env.temperatureC = -100.0f;
  reading.env.humidityPct = 100.0f;
  reading.env.illuminanceLux = 65535.0f;

  char buf[kMaxPayloadLen];
  TEST_ASSERT_GREATER_THAN(0, buildPayload(reading, buf, sizeof(buf)));
}

void test_buffer_size_boundary() {
  const size_t length = strlen(kWithoutSensors);
  char buf[kMaxPayloadLen];
  TEST_ASSERT_EQUAL(length, buildPayload(sampleReading(), buf, length + 1));
  TEST_ASSERT_EQUAL(0, buildPayload(sampleReading(), buf, length));
  TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_zero_capacity_is_a_failure_without_writing() {
  TEST_ASSERT_EQUAL(0, buildPayload(sampleReading(), nullptr, 0));
}

void test_device_id_must_be_a_plain_identifier() {
  char buf[kMaxPayloadLen];
  for (const char* bad : {"", "has space", "quote\"", "back\\slash", "new\nline"}) {
    Reading reading = sampleReading();
    strcpy(reading.deviceId, bad);
    TEST_ASSERT_EQUAL(0, buildPayload(reading, buf, sizeof(buf)));
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_reading_without_sensors);
  RUN_TEST(test_reading_with_all_sensors);
  RUN_TEST(test_missing_sensor_values_are_omitted_one_by_one);
  RUN_TEST(test_negative_sensor_values);
  RUN_TEST(test_largest_reading_fits_the_maximum_length);
  RUN_TEST(test_buffer_size_boundary);
  RUN_TEST(test_zero_capacity_is_a_failure_without_writing);
  RUN_TEST(test_device_id_must_be_a_plain_identifier);
  return UNITY_END();
}
