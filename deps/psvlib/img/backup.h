/* Copyright (C) 2017 Yifan Lu
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#ifndef BACKUP_H__
#define BACKUP_H__

#include <stdint.h>

typedef struct args {
  int in;
  int out;
  unsigned char key[32];
  unsigned char iv[16];
  const char *prefix;
  size_t content_size;
} args_t;

void *encrypt_thread(void *pargs);
void *pack_thread(void *pargs);

int compress_and_encrypt(const char* output, const uint8_t* data, size_t data_size, uint8_t iv[16], uint8_t key[32]);
int pack_and_encrypt(const char* output, uint64_t* content_size, const char* prefix, uint8_t iv[16], uint8_t key[32]);

#endif
