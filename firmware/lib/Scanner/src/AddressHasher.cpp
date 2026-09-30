#include "AddressHasher.h"

#include <mbedtls/platform_util.h>
#include <mbedtls/sha256.h>
#include <string.h>

namespace AddressHasher {

uint64_t hash(const uint8_t* salt, size_t saltLen, const uint8_t* address,
              uint8_t addressType) {
  uint8_t input[kMaxSaltLen + 1 + kAddressLen];
  if (saltLen > kMaxSaltLen) saltLen = kMaxSaltLen;

  size_t len = 0;
  memcpy(input + len, salt, saltLen);
  len += saltLen;
  input[len++] = addressType;
  memcpy(input + len, address, kAddressLen);
  len += kAddressLen;

  uint8_t digest[32];
  mbedtls_sha256(input, len, digest, 0);

  uint64_t truncated;
  memcpy(&truncated, digest, sizeof(truncated));

  // memset here could be optimized away as a dead store
  mbedtls_platform_zeroize(input, sizeof(input));
  mbedtls_platform_zeroize(digest, sizeof(digest));
  return truncated;
}

}  // namespace AddressHasher
