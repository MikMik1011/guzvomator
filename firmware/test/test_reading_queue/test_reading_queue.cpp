#include <unity.h>

#include <initializer_list>

#include "ReadingQueue.h"

void setUp() {}
void tearDown() {}

namespace {

Reading readingWithTs(uint32_t ts) {
  Reading reading;
  reading.ts = ts;
  return reading;
}

}  // namespace

void test_a_new_queue_is_empty() {
  ReadingQueue queue;
  TEST_ASSERT_TRUE(queue.empty());
  TEST_ASSERT_EQUAL(0, queue.size());
}

void test_readings_come_out_oldest_first() {
  ReadingQueue queue;
  queue.push(readingWithTs(1));
  queue.push(readingWithTs(2));
  queue.push(readingWithTs(3));

  for (uint32_t expected : {1u, 2u, 3u}) {
    TEST_ASSERT_EQUAL_UINT32(expected, queue.front().ts);
    queue.pop();
  }
  TEST_ASSERT_TRUE(queue.empty());
}

void test_pop_on_an_empty_queue_is_harmless() {
  ReadingQueue queue;
  queue.pop();
  TEST_ASSERT_EQUAL(0, queue.size());
  queue.push(readingWithTs(7));
  TEST_ASSERT_EQUAL_UINT32(7, queue.front().ts);
}

void test_a_full_queue_drops_the_oldest() {
  ReadingQueue queue;
  for (uint32_t i = 1; i <= ReadingQueue::kCapacity; i++) {
    TEST_ASSERT_FALSE(queue.push(readingWithTs(i)));
  }
  TEST_ASSERT_EQUAL(ReadingQueue::kCapacity, queue.size());

  TEST_ASSERT_TRUE(queue.push(readingWithTs(100)));
  TEST_ASSERT_TRUE(queue.push(readingWithTs(101)));
  TEST_ASSERT_EQUAL(ReadingQueue::kCapacity, queue.size());
  TEST_ASSERT_EQUAL_UINT32(3, queue.front().ts);
}

void test_order_survives_wrapping_around_the_buffer() {
  ReadingQueue queue;
  uint32_t nextToPush = 1;
  uint32_t nextExpected = 1;
  for (int round = 0; round < 5; round++) {
    for (size_t i = 0; i < ReadingQueue::kCapacity - 3; i++) {
      queue.push(readingWithTs(nextToPush++));
    }
    for (size_t i = 0; i < ReadingQueue::kCapacity - 3; i++) {
      TEST_ASSERT_EQUAL_UINT32(nextExpected++, queue.front().ts);
      queue.pop();
    }
  }
  TEST_ASSERT_TRUE(queue.empty());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_new_queue_is_empty);
  RUN_TEST(test_readings_come_out_oldest_first);
  RUN_TEST(test_pop_on_an_empty_queue_is_harmless);
  RUN_TEST(test_a_full_queue_drops_the_oldest);
  RUN_TEST(test_order_survives_wrapping_around_the_buffer);
  return UNITY_END();
}
