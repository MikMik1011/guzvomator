#include <string.h>
#include <unity.h>

#include "LineBuffer.h"

void setUp() {}
void tearDown() {}

namespace {

// Feeds text and returns the event of the last character.
LineBuffer::Event feedAll(LineBuffer& buffer, const char* text) {
  LineBuffer::Event last = LineBuffer::Event::None;
  for (const char* c = text; *c != '\0'; c++) last = buffer.feed(*c);
  return last;
}

}  // namespace

void test_a_line_is_ready_at_newline() {
  LineBuffer buffer;
  TEST_ASSERT_EQUAL(LineBuffer::Event::None, feedAll(buffer, "set a 1"));
  TEST_ASSERT_EQUAL(LineBuffer::Event::LineReady, buffer.feed('\n'));
  TEST_ASSERT_EQUAL_STRING("set a 1", buffer.text());
}

void test_carriage_return_is_ignored() {
  LineBuffer buffer;
  feedAll(buffer, "list\r");
  TEST_ASSERT_EQUAL(LineBuffer::Event::LineReady, buffer.feed('\n'));
  TEST_ASSERT_EQUAL_STRING("list", buffer.text());
}

void test_backspace_erases_the_previous_character() {
  LineBuffer buffer;
  feedAll(buffer, "set wifi0_ssid p\bblokadapipko");
  buffer.feed('\n');
  TEST_ASSERT_EQUAL_STRING("set wifi0_ssid blokadapipko", buffer.text());
}

void test_delete_key_erases_like_backspace() {
  LineBuffer buffer;
  feedAll(buffer, "lisx\x7f"
                  "t");
  buffer.feed('\n');
  TEST_ASSERT_EQUAL_STRING("list", buffer.text());
}

void test_backspace_on_an_empty_line_does_nothing() {
  LineBuffer buffer;
  feedAll(buffer, "\b\b");
  feedAll(buffer, "ok");
  buffer.feed('\n');
  TEST_ASSERT_EQUAL_STRING("ok", buffer.text());
}

void test_tab_becomes_a_space() {
  LineBuffer buffer;
  feedAll(buffer, "get\twifi0_ssid");
  buffer.feed('\n');
  TEST_ASSERT_EQUAL_STRING("get wifi0_ssid", buffer.text());
}

void test_arrow_keys_are_skipped() {
  LineBuffer buffer;
  feedAll(buffer, "li\x1b[Ast\x1b[D");
  buffer.feed('\n');
  TEST_ASSERT_EQUAL_STRING("list", buffer.text());
}

void test_other_control_characters_are_dropped() {
  LineBuffer buffer;
  feedAll(buffer, "a\x01\x02" "b\x07");
  buffer.feed('\n');
  TEST_ASSERT_EQUAL_STRING("ab", buffer.text());
}

void test_empty_lines_produce_no_event() {
  LineBuffer buffer;
  TEST_ASSERT_EQUAL(LineBuffer::Event::None, buffer.feed('\n'));
  TEST_ASSERT_EQUAL(LineBuffer::Event::None, feedAll(buffer, "\r\n"));
}

void test_two_lines_in_a_row() {
  LineBuffer buffer;
  feedAll(buffer, "first\n");
  TEST_ASSERT_EQUAL_STRING("first", buffer.text());
  feedAll(buffer, "second");
  TEST_ASSERT_EQUAL(LineBuffer::Event::LineReady, buffer.feed('\n'));
  TEST_ASSERT_EQUAL_STRING("second", buffer.text());
}

void test_a_line_at_the_capacity_limit_is_accepted() {
  LineBuffer buffer;
  for (size_t i = 0; i < LineBuffer::kCapacity - 1; i++) buffer.feed('x');
  TEST_ASSERT_EQUAL(LineBuffer::Event::LineReady, buffer.feed('\n'));
  TEST_ASSERT_EQUAL(LineBuffer::kCapacity - 1, strlen(buffer.text()));
}

void test_a_longer_line_is_rejected_and_the_next_one_works() {
  LineBuffer buffer;
  for (size_t i = 0; i < LineBuffer::kCapacity + 20; i++) buffer.feed('x');
  TEST_ASSERT_EQUAL(LineBuffer::Event::TooLong, buffer.feed('\n'));

  feedAll(buffer, "ok");
  TEST_ASSERT_EQUAL(LineBuffer::Event::LineReady, buffer.feed('\n'));
  TEST_ASSERT_EQUAL_STRING("ok", buffer.text());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_line_is_ready_at_newline);
  RUN_TEST(test_carriage_return_is_ignored);
  RUN_TEST(test_backspace_erases_the_previous_character);
  RUN_TEST(test_delete_key_erases_like_backspace);
  RUN_TEST(test_backspace_on_an_empty_line_does_nothing);
  RUN_TEST(test_tab_becomes_a_space);
  RUN_TEST(test_arrow_keys_are_skipped);
  RUN_TEST(test_other_control_characters_are_dropped);
  RUN_TEST(test_empty_lines_produce_no_event);
  RUN_TEST(test_two_lines_in_a_row);
  RUN_TEST(test_a_line_at_the_capacity_limit_is_accepted);
  RUN_TEST(test_a_longer_line_is_rejected_and_the_next_one_works);
  return UNITY_END();
}
