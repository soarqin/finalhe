/* Copyright (C) 2017 Yifan Lu
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#ifndef CRYPTO_H__
#define CRYPTO_H__

#include <stdint.h>

#define AES_BLOCK_SIZE 16
#define SHA256_BLOCK_SIZE 32

#include <aes.h>
static inline void aes256_cbc_decrypt(uint8_t *buffer, uint8_t *key, uint8_t *iv, size_t blocks) {
  aes_context ctx;

  aes_init(&ctx, key, 256);
  for (int i = blocks-1; i >= 0; i--) {
      aes_ecb_decrypt(&ctx, &buffer[i * AES_BLOCK_SIZE], &buffer[i * AES_BLOCK_SIZE]);
    for (int j = 0; j < AES_BLOCK_SIZE; j++) {
      buffer[i*AES_BLOCK_SIZE + j] ^= (i == 0) ? iv[j] : buffer[(i-1)*AES_BLOCK_SIZE + j];
    }
  }
}

static inline void aes256_cbc_encrypt(uint8_t *buffer, uint8_t *key, uint8_t *iv, size_t blocks) {
  aes_context ctx;

  aes_init(&ctx, key, 256);
  for (int i = 0; i < blocks; i++) {
    for (int j = 0; j < AES_BLOCK_SIZE; j++) {
      buffer[i*AES_BLOCK_SIZE + j] ^= (i == 0) ? iv[j] : buffer[(i-1)*AES_BLOCK_SIZE + j];
    }
    aes_ecb_encrypt(&ctx, &buffer[i * AES_BLOCK_SIZE], &buffer[i * AES_BLOCK_SIZE]);
  }
}

static inline void aes256_encrypt(uint8_t *buffer, uint8_t *key) {
  aes_context ctx;

  aes_init(&ctx, key, 256);
  aes_ecb_encrypt(&ctx, buffer, buffer);
}

#include <sha256.h>

#endif
