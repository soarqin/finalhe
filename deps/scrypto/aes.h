#pragma once

#include <stdint.h>
#include <stddef.h>
#if defined(_MSC_VER)
#  define _ALIGNED(x) __declspec(align(x))
#else
#  define _ALIGNED(x) __attribute__((aligned(x)))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct aes_context {
    int nr;
    uint32_t _ALIGNED(16) key[68];
} aes_context;

int aes_init(aes_context* ctx, const uint8_t* key, int bits);
int aes_init_dec(aes_context* ctx, const uint8_t* key, int bits);

void aes_ecb_encrypt(const aes_context* ctx, const uint8_t* input, uint8_t* output);
void aes_ecb_decrypt(const aes_context* ctx, const uint8_t* input, uint8_t* output);

int aes_cbc_encrypt(const aes_context* ctx, uint8_t iv[16], const uint8_t* input, size_t length, uint8_t* output);
int aes_cbc_decrypt(const aes_context* ctx, uint8_t iv[16], const uint8_t* input, size_t length, uint8_t* output);

void aes_ctr_xor(const aes_context* ctx, const uint8_t* iv, uint64_t block, uint8_t* buffer, size_t size);

void aes_cmac(const uint8_t* key, const uint8_t* buffer, uint32_t size, uint8_t* mac);

void aes_psp_decrypt(const aes_context* ctx, const uint8_t* iv, uint32_t index, uint8_t* buffer, uint32_t size);

#ifdef __cplusplus
}
#endif
