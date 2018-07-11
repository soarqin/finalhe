#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t total[2];          /*!< The number of Bytes processed.  */
    uint32_t state[8];          /*!< The intermediate digest state.  */
    unsigned char buffer[64];   /*!< The data block being processed. */
}
sha256_context;

void sha256_init(sha256_context *ctx);

int sha256_starts(sha256_context *ctx);

int sha256_update(sha256_context *ctx,
    const unsigned char *input,
    size_t ilen);

int sha256_final(sha256_context *ctx,
    unsigned char output[32]);

void sha256_vector(size_t num_elem, const uint8_t *addr[], const size_t *len,
         uint8_t *mac);

void sha256_copy(sha256_context *to, sha256_context *from);

#ifdef __cplusplus
}
#endif
