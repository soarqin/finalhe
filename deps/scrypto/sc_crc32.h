#pragma once

#include <stdint.h>
#if defined(_MSC_VER)
#  define _ALIGNED(x) __declspec(align(x))
#else
#  define _ALIGNED(x) __attribute__((aligned(x)))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t _ALIGNED(16) crc[4 * 5];
} crc32_ctx;

void scrypto_crc32_init(crc32_ctx* ctx);
void scrypto_crc32_update(crc32_ctx* ctx, const void* buffer, size_t size);
uint32_t scrypto_crc32_done(crc32_ctx* ctx);

// returns crc32(x||y)
// where a=crc32(x) and b=crc32(y)
// crc32(x||y) = crc32(x||z) ^ crc32(y), where z=00...00 (same length as y)
uint32_t scrypto_crc32_combine(uint32_t a, uint32_t b, uint32_t blen);

#ifdef __cplusplus
}
#endif
