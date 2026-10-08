#pragma once
#include <stddef.h>

#include "Reading.h"

// Fixed-size FIFO of readings waiting to be sent. Full means the oldest goes.
class ReadingQueue {
 public:
  static constexpr size_t kCapacity = 20;

  // Returns true if the oldest reading was dropped to make room.
  bool push(const Reading& reading) {
    bool droppedOldest = false;
    if (size_ == kCapacity) {
      head_ = (head_ + 1) % kCapacity;
      size_--;
      droppedOldest = true;
    }
    items_[(head_ + size_) % kCapacity] = reading;
    size_++;
    return droppedOldest;
  }

  bool empty() const { return size_ == 0; }
  size_t size() const { return size_; }
  const Reading& front() const { return items_[head_]; }

  void pop() {
    if (size_ == 0) return;
    head_ = (head_ + 1) % kCapacity;
    size_--;
  }

 private:
  Reading items_[kCapacity];
  size_t head_ = 0;
  size_t size_ = 0;
};
