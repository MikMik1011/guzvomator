#pragma once
#include <stddef.h>

// Collects typed characters into a line, the way a terminal would show it.
class LineBuffer {
 public:
  enum class Event { None, LineReady, TooLong };

  static constexpr size_t kCapacity = 192;

  // text() stays valid until the next call to feed().
  Event feed(char c) {
    if (finished_) startNewLine();

    if (inEscape_) {
      endEscapeOnFinalByte(c);
      return Event::None;
    }

    switch (c) {
      case '\r':
        return Event::None;
      case '\n':
        return finishLine();
      case '\b':
      case 0x7f:
        if (!overflowed_ && length_ > 0) length_--;
        return Event::None;
      case 0x1b:
        inEscape_ = true;
        return Event::None;
      case '\t':
        c = ' ';
        break;
      default:
        break;
    }

    if (static_cast<unsigned char>(c) < 0x20) return Event::None;

    if (length_ < kCapacity - 1) {
      line_[length_++] = c;
    } else {
      overflowed_ = true;
    }
    return Event::None;
  }

  char* text() { return line_; }

 private:
  void startNewLine() {
    length_ = 0;
    overflowed_ = false;
    finished_ = false;
  }

  Event finishLine() {
    finished_ = true;
    if (overflowed_) return Event::TooLong;
    if (length_ == 0) return Event::None;
    line_[length_] = '\0';
    return Event::LineReady;
  }

  // Skips arrow keys and similar: ESC [ ... final byte
  void endEscapeOnFinalByte(char c) {
    const bool isFinalByte = c >= '@' && c <= '~' && c != '[';
    if (isFinalByte) inEscape_ = false;
  }

  char line_[kCapacity] = "";
  size_t length_ = 0;
  bool overflowed_ = false;
  bool inEscape_ = false;
  bool finished_ = false;
};
