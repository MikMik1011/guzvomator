#include <unity.h>

#include <initializer_list>

#include "HttpStatus.h"

void setUp() {}
void tearDown() {}

void test_created_and_duplicate_count_as_sent() {
  TEST_ASSERT_EQUAL(SendResult::Sent, classifyHttpStatus(201));
  TEST_ASSERT_EQUAL(SendResult::Sent, classifyHttpStatus(200));
}

void test_an_invalid_payload_is_rejected_for_good() {
  TEST_ASSERT_EQUAL(SendResult::Rejected, classifyHttpStatus(400));
}

void test_everything_else_is_worth_retrying() {
  for (int status : {401, 403, 404, 405, 413, 422, 429, 500, 502, 503, 504}) {
    TEST_ASSERT_EQUAL(SendResult::Retry, classifyHttpStatus(status));
  }
}

void test_connection_failures_are_retried() {
  TEST_ASSERT_EQUAL(SendResult::Retry, classifyHttpStatus(0));
  TEST_ASSERT_EQUAL(SendResult::Retry, classifyHttpStatus(-1));
  TEST_ASSERT_EQUAL(SendResult::Retry, classifyHttpStatus(-11));
}

void test_other_success_codes_are_not_assumed_stored() {
  TEST_ASSERT_EQUAL(SendResult::Retry, classifyHttpStatus(202));
  TEST_ASSERT_EQUAL(SendResult::Retry, classifyHttpStatus(204));
  TEST_ASSERT_EQUAL(SendResult::Retry, classifyHttpStatus(301));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_created_and_duplicate_count_as_sent);
  RUN_TEST(test_an_invalid_payload_is_rejected_for_good);
  RUN_TEST(test_everything_else_is_worth_retrying);
  RUN_TEST(test_connection_failures_are_retried);
  RUN_TEST(test_other_success_codes_are_not_assumed_stored);
  return UNITY_END();
}
