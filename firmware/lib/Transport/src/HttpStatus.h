#pragma once

#include "ITransport.h"

// 200 is a duplicate and 201 a new reading; both mean the backend has it.
// 400 means the payload is invalid. Anything else, including a missing
// connection (zero or negative), is worth another try.
inline SendResult classifyHttpStatus(int status) {
  if (status == 200 || status == 201) return SendResult::Sent;
  if (status == 400) return SendResult::Rejected;
  return SendResult::Retry;
}
