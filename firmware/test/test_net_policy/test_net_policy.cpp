#include <unity.h>

#include "NetPolicy.h"

void setUp() {}
void tearDown() {}

void test_a_new_backoff_is_due_immediately() {
  Backoff backoff(5000, 60000);
  TEST_ASSERT_TRUE(backoff.due(0));
  TEST_ASSERT_TRUE(backoff.due(123456));
}

void test_failure_delays_the_next_attempt() {
  Backoff backoff(5000, 60000);
  backoff.failed(1000);
  TEST_ASSERT_FALSE(backoff.due(1000));
  TEST_ASSERT_FALSE(backoff.due(5999));
  TEST_ASSERT_TRUE(backoff.due(6000));
}

void test_delay_doubles_up_to_the_maximum() {
  Backoff backoff(5000, 60000);
  uint32_t now = 0;
  const uint32_t expected[] = {5000, 10000, 20000, 40000, 60000, 60000};
  for (uint32_t delay : expected) {
    backoff.failed(now);
    TEST_ASSERT_FALSE(backoff.due(now + delay - 1));
    TEST_ASSERT_TRUE(backoff.due(now + delay));
    now += delay;
  }
}

void test_reset_makes_it_due_and_restarts_the_schedule() {
  Backoff backoff(5000, 60000);
  backoff.failed(0);
  backoff.failed(5000);
  backoff.reset();
  TEST_ASSERT_TRUE(backoff.due(5001));
  backoff.failed(10000);
  TEST_ASSERT_FALSE(backoff.due(14999));
  TEST_ASSERT_TRUE(backoff.due(15000));
}

void test_backoff_survives_the_millis_wraparound() {
  Backoff backoff(5000, 60000);
  const uint32_t beforeWrap = 0xFFFFFF00u;  // retry lands at 4744 after the wrap
  backoff.failed(beforeWrap);
  TEST_ASSERT_FALSE(backoff.due(0xFFFFFFF0u));
  TEST_ASSERT_FALSE(backoff.due(100));
  TEST_ASSERT_TRUE(backoff.due(4744));
}

void test_plausible_epoch_boundary() {
  TEST_ASSERT_FALSE(isPlausibleEpoch(0));
  TEST_ASSERT_FALSE(isPlausibleEpoch(-1));
  TEST_ASSERT_FALSE(isPlausibleEpoch(1'577'836'799));
  TEST_ASSERT_TRUE(isPlausibleEpoch(1'577'836'800));
  TEST_ASSERT_TRUE(isPlausibleEpoch(1'790'000'000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_new_backoff_is_due_immediately);
  RUN_TEST(test_failure_delays_the_next_attempt);
  RUN_TEST(test_delay_doubles_up_to_the_maximum);
  RUN_TEST(test_reset_makes_it_due_and_restarts_the_schedule);
  RUN_TEST(test_backoff_survives_the_millis_wraparound);
  RUN_TEST(test_plausible_epoch_boundary);
  return UNITY_END();
}
