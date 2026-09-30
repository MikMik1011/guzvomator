#pragma once
#include <stddef.h>
#include <stdint.h>

namespace AddressHasher {

constexpr size_t kAddressLen = 6;
constexpr size_t kMaxSaltLen = 32;

// SHA-256 over salt || address type || address, truncated to 64 bits.
uint64_t hash(const uint8_t* salt, size_t saltLen, const uint8_t* address,
              uint8_t addressType);

}  // namespace AddressHasher
