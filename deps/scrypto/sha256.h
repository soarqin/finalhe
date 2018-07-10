#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint8_t data[64];
   uint32_t datalen;
   uint32_t bitlen[2];
   uint32_t state[8];
} SHA256_CTX;

void sha256_transform(SHA256_CTX *ctx, const uint8_t data[]);
void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const uint8_t data[], uint32_t len);
void sha256_final(SHA256_CTX *ctx, uint8_t hash[]);
void sha256_vector(size_t num_elem, const uint8_t *addr[], const size_t *len,
         uint8_t *mac);

#ifdef __cplusplus
}
#endif
