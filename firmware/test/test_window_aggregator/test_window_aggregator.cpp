#include <unity.h>

#include "WindowAggregator.h"

void setUp() {}
void tearDown() {}

void test_empty_window_is_all_zero() {
  WindowAggregator window;
  const ScanResult result = window.summarize(-80);

  TEST_ASSERT_EQUAL_UINT16(0, result.uniqueDevices);
  TEST_ASSERT_EQUAL_UINT16(0, result.uniqueDevicesAboveRssi);
  TEST_ASSERT_EQUAL_INT8(0, result.avgRssi);
}

void test_repeated_hash_counts_once_with_mean_rssi() {
  WindowAggregator window;
  window.add(1, -70);
  window.add(1, -80);
  const ScanResult result = window.summarize(-80);

  TEST_ASSERT_EQUAL_UINT16(1, result.uniqueDevices);
  TEST_ASSERT_EQUAL_INT8(-75, result.avgRssi);
}

void test_threshold_uses_per_device_mean() {
  WindowAggregator window;
  window.add(1, -60);
  window.add(2, -90);

  TEST_ASSERT_EQUAL_UINT16(1, window.summarize(-70).uniqueDevicesAboveRssi);
  TEST_ASSERT_EQUAL_UINT16(2, window.summarize(-90).uniqueDevicesAboveRssi);
  TEST_ASSERT_EQUAL_UINT16(0, window.summarize(-50).uniqueDevicesAboveRssi);
}

void test_average_is_mean_of_device_means() {
  WindowAggregator window;
  for (int i = 0; i < 10; i++) window.add(1, -50);  // chatty device
  window.add(2, -90);

  TEST_ASSERT_EQUAL_INT8(-70, window.summarize(-80).avgRssi);
}

void test_clear_resets_window() {
  WindowAggregator window;
  window.add(1, -60);
  window.clear();

  TEST_ASSERT_EQUAL_UINT16(0, window.summarize(-80).uniqueDevices);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_empty_window_is_all_zero);
  RUN_TEST(test_repeated_hash_counts_once_with_mean_rssi);
  RUN_TEST(test_threshold_uses_per_device_mean);
  RUN_TEST(test_average_is_mean_of_device_means);
  RUN_TEST(test_clear_resets_window);
  return UNITY_END();
}
