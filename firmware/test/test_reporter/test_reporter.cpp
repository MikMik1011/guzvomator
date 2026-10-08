#include <string.h>
#include <unity.h>

#include <string>
#include <vector>

#include "Reporter.h"

void setUp() {}
void tearDown() {}

namespace {

// Answers from a script, then keeps repeating the last answer.
class FakeTransport : public ITransport {
 public:
  explicit FakeTransport(std::vector<SendResult> script) : script_(script) {}

  SendOutcome send(const char* json, size_t len) override {
    payloads.emplace_back(json, len);
    const SendResult result =
        script_[calls_ < script_.size() ? calls_ : script_.size() - 1];
    calls_++;
    return {result, 0};
  }

  std::vector<std::string> payloads;

 private:
  std::vector<SendResult> script_;
  size_t calls_ = 0;
};

Reading readingWithTs(uint32_t ts) {
  Reading reading;
  strcpy(reading.deviceId, "guzvo-test");
  reading.ts = ts;
  return reading;
}

bool contains(const std::string& text, const char* part) {
  return text.find(part) != std::string::npos;
}

}  // namespace

void test_a_reading_is_sent_and_leaves_the_queue() {
  Reporter reporter;
  reporter.submit(readingWithTs(1790000000));
  FakeTransport transport({SendResult::Sent});

  const Reporter::FlushSummary summary = reporter.flush(transport, 0);
  TEST_ASSERT_EQUAL(1, summary.sent);
  TEST_ASSERT_FALSE(summary.stalled);
  TEST_ASSERT_EQUAL(0, reporter.queued());
  TEST_ASSERT_EQUAL_UINT32(1, reporter.stats().sent);
  TEST_ASSERT_EQUAL(1, transport.payloads.size());
  TEST_ASSERT_TRUE(contains(transport.payloads[0], "\"ts\":1790000000"));
  TEST_ASSERT_TRUE(contains(transport.payloads[0], "\"device_id\":\"guzvo-test\""));
}

void test_the_queue_is_sent_oldest_first() {
  Reporter reporter;
  for (uint32_t ts : {100u, 200u, 300u}) reporter.submit(readingWithTs(ts));
  FakeTransport transport({SendResult::Sent});

  TEST_ASSERT_EQUAL(3, reporter.flush(transport, 0).sent);
  TEST_ASSERT_TRUE(contains(transport.payloads[0], "\"ts\":100"));
  TEST_ASSERT_TRUE(contains(transport.payloads[1], "\"ts\":200"));
  TEST_ASSERT_TRUE(contains(transport.payloads[2], "\"ts\":300"));
}

void test_a_retryable_failure_stops_and_keeps_everything() {
  Reporter reporter;
  for (uint32_t ts : {100u, 200u, 300u}) reporter.submit(readingWithTs(ts));
  FakeTransport transport({SendResult::Sent, SendResult::Retry});

  const Reporter::FlushSummary summary = reporter.flush(transport, 0);
  TEST_ASSERT_EQUAL(1, summary.sent);
  TEST_ASSERT_TRUE(summary.stalled);
  TEST_ASSERT_EQUAL(2, reporter.queued());
  TEST_ASSERT_EQUAL(2, transport.payloads.size());
}

void test_nothing_is_sent_while_the_backoff_runs() {
  Reporter reporter;
  reporter.submit(readingWithTs(100));
  FakeTransport failing({SendResult::Retry});
  reporter.flush(failing, 1000);

  FakeTransport working({SendResult::Sent});
  TEST_ASSERT_EQUAL(0, reporter.flush(working, 1000 + 4999).sent);
  TEST_ASSERT_EQUAL(0, working.payloads.size());
  TEST_ASSERT_EQUAL(1, reporter.flush(working, 1000 + 5000).sent);
  TEST_ASSERT_EQUAL(0, reporter.queued());
}

void test_the_retry_wait_grows_and_resets_after_a_success() {
  Reporter reporter;
  reporter.submit(readingWithTs(100));
  FakeTransport failing({SendResult::Retry});

  reporter.flush(failing, 0);       // next try after 5 s
  reporter.flush(failing, 5000);    // next try after 10 s
  TEST_ASSERT_EQUAL(2, failing.payloads.size());

  reporter.flush(failing, 14999);   // still waiting
  TEST_ASSERT_EQUAL(2, failing.payloads.size());
  FakeTransport working({SendResult::Sent});
  TEST_ASSERT_EQUAL(1, reporter.flush(working, 15000).sent);

  reporter.submit(readingWithTs(200));
  FakeTransport failingAgain({SendResult::Retry});
  reporter.flush(failingAgain, 20000);
  FakeTransport again({SendResult::Sent});
  TEST_ASSERT_EQUAL(1, reporter.flush(again, 25000).sent);  // back to a 5 s wait
}

void test_a_rejected_reading_is_dropped_and_the_rest_continue() {
  Reporter reporter;
  for (uint32_t ts : {100u, 200u, 300u}) reporter.submit(readingWithTs(ts));
  FakeTransport transport({SendResult::Sent, SendResult::Rejected, SendResult::Sent});

  const Reporter::FlushSummary summary = reporter.flush(transport, 0);
  TEST_ASSERT_EQUAL(2, summary.sent);
  TEST_ASSERT_EQUAL(1, summary.rejected);
  TEST_ASSERT_FALSE(summary.stalled);
  TEST_ASSERT_EQUAL(0, reporter.queued());
  TEST_ASSERT_EQUAL_UINT32(1, reporter.stats().rejected);
}

void test_a_reading_that_cannot_be_serialized_is_rejected_without_sending() {
  Reporter reporter;
  Reading bad = readingWithTs(100);
  strcpy(bad.deviceId, "bad id");
  reporter.submit(bad);
  FakeTransport transport({SendResult::Sent});

  const Reporter::FlushSummary summary = reporter.flush(transport, 0);
  TEST_ASSERT_EQUAL(1, summary.rejected);
  TEST_ASSERT_EQUAL(0, transport.payloads.size());
  TEST_ASSERT_EQUAL(0, reporter.queued());
}

void test_overflow_drops_the_oldest_and_counts_it() {
  Reporter reporter;
  for (uint32_t i = 0; i < ReadingQueue::kCapacity + 3; i++) {
    reporter.submit(readingWithTs(1000 + i));
  }
  TEST_ASSERT_EQUAL(ReadingQueue::kCapacity, reporter.queued());
  TEST_ASSERT_EQUAL_UINT32(3, reporter.stats().dropped);

  FakeTransport transport({SendResult::Sent});
  reporter.flush(transport, 0);
  TEST_ASSERT_TRUE(contains(transport.payloads[0], "\"ts\":1003"));
}

void test_flushing_an_empty_queue_does_nothing() {
  Reporter reporter;
  FakeTransport transport({SendResult::Sent});
  const Reporter::FlushSummary summary = reporter.flush(transport, 0);
  TEST_ASSERT_EQUAL(0, summary.sent);
  TEST_ASSERT_FALSE(summary.stalled);
  TEST_ASSERT_EQUAL(0, transport.payloads.size());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_reading_is_sent_and_leaves_the_queue);
  RUN_TEST(test_the_queue_is_sent_oldest_first);
  RUN_TEST(test_a_retryable_failure_stops_and_keeps_everything);
  RUN_TEST(test_nothing_is_sent_while_the_backoff_runs);
  RUN_TEST(test_the_retry_wait_grows_and_resets_after_a_success);
  RUN_TEST(test_a_rejected_reading_is_dropped_and_the_rest_continue);
  RUN_TEST(test_a_reading_that_cannot_be_serialized_is_rejected_without_sending);
  RUN_TEST(test_overflow_drops_the_oldest_and_counts_it);
  RUN_TEST(test_flushing_an_empty_queue_does_nothing);
  return UNITY_END();
}
