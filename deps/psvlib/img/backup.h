/* Copyright (C) 2017 Yifan Lu
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#ifndef BACKUP_H__
#define BACKUP_H__

typedef struct args {
  int in;
  int out;
  unsigned char key[32];
  unsigned char iv[16];
  const char *prefix;
  size_t content_size;
} args_t;

void *encrypt_thread(void *pargs);
void *compress_thread(void *pargs);
void *pack_thread(void *pargs);

#endif
