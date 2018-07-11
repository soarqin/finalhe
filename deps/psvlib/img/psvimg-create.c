/* Copyright (C) 2017 Yifan Lu
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include "psvimg-create.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#ifdef _MSC_VER
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#define pipe(n) _pipe(n, 256 * 1024, _O_BINARY)
#define mkdir(n, m) _mkdir(n)
#else
#include <unistd.h>
#endif
#ifdef _WIN32
#include "pthread-win32.h"
#else
#include <pthread.h>
#endif
#include "backup.h"
#include "psvimg.h"
#include "utils.h"

#define MAX_PATH_LEN 1024

static int is_backup(const char *title) {
  if (strcmp(title, "app") == 0 ||
      strcmp(title, "patch") == 0 ||
      strcmp(title, "addcont") == 0 ||
      strcmp(title, "savedata") == 0 ||
      strcmp(title, "appmeta") == 0 ||
      strcmp(title, "license") == 0 ||
      strcmp(title, "game") == 0) {
    return 0;
  } else {
    return 1;
  }
}

int psvimg_create(const char *inputdir, const char *outputdir, const char *key, const char *meta_name, int is_meta) {
  args_t args1, args2;
  struct stat st;
  int fds[4];
  char path[MAX_PATH_LEN];
  int mfd;
  PsvMd_t md;
  pthread_t th1, th2;

  // TODO: support more types
  if (is_meta) {
    mfd = open(meta_name, O_RDONLY | O_BINARY);
    if (mfd < 0) {
      fprintf(stderr, "metadata");
      return 1;
    }
    if (read_block(mfd, &md, sizeof(md) - sizeof(md.add_data)) < sizeof(md) - sizeof(md.add_data)) {
      fprintf(stderr, "invalid metadata size\n");
      return 1;
    }
    if (md.type != 2) {
      fprintf(stderr, "metadata type not supported\n");
      close(mfd);
      return 1;
    }
    if (read_block(mfd, &md.add_data, sizeof(md.add_data)) < sizeof(md.add_data)) {
      fprintf(stderr, "invalid metadata size\n");
      close(mfd);
      return 1;
    }
    close(mfd);
  } else {
    memset(&md, 0, sizeof(md));
    strncpy(md.name, meta_name, sizeof(md.name));
    md.magic = is_backup(md.name) ? PSVMD_BACKUP_MAGIC : PSVMD_CONTENT_MAGIC;
    md.type = 2;
    md.version = 2;
    md.add_data = 1;
    srand(time(NULL));
    for (int i = 0; i < sizeof(md.iv); i++) {
      md.iv[i] = rand() % 0xFF;
    }
  }

  if (pipe(fds) < 0) {
    fprintf(stderr, "pipe 1");
    return 1;
  }

  args1.in = 0;
  args1.prefix = inputdir;
  args1.content_size = 0;
  args1.out = fds[1];

  if (stat(outputdir, &st) < 0) {
    mkdir(outputdir, 0700);
  }

  snprintf(path, sizeof(path), "%s/%s.psvimg", outputdir, md.name);

  args2.out = open(path, O_BINARY | O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (args2.out < 0) {
    fprintf(stderr, "psvimg output");
    return 1;
  }
  args2.in = fds[0];

  if (parse_key(key, args2.key) < 0) {
    fprintf(stderr, "invalid key\n");
    return 1;
  }

  memcpy(args2.iv, md.iv, sizeof(args2.iv));

  if (pthread_create(&th1, NULL, pack_thread, &args1) != 0) {
    fprintf(stderr, "unable to create thread");
    return 1;
  }
  encrypt_thread(&args2);
  pthread_join(th1, NULL);

  if (stat(path, &st) < 0) {
    fprintf(stderr, "stat");
    return 1;
  }
  fprintf(stderr, "created %s (size: %"
#ifdef _MSC_VER
      "l"
#else
      "ll"
#endif
      "x, content size: %zx)\n", path, st.st_size, args1.content_size);
  md.total_size = args1.content_size;
  md.psvimg_size = st.st_size;

  // now create the psvmd
  snprintf(path, sizeof(path), "%s/%s.psvmd", outputdir, md.name);

  if (pipe(fds) < 0) {
    fprintf(stderr, "pipe 2");
    return 1;
  }
  if (pipe(&fds[2]) < 0) {
    fprintf(stderr, "pipe 3");
    return 1;
  }

  args1.in = fds[0];
  args1.out = fds[3];

  args2.in = fds[2];
  args2.out = open(path, O_BINARY | O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (args2.out < 0) {
    fprintf(stderr, "psvmd output");
    return 1;
  }
  for (int i = 0; i < sizeof(md.iv); i++) {
    args2.iv[i] = rand() % 0xFF;
  }

  if (pthread_create(&th1, NULL, encrypt_thread, &args2) != 0) {
    fprintf(stderr, "unable to create thread");
    return 1;
  }
  if (pthread_create(&th2, NULL, compress_thread, &args1) != 0) {
    fprintf(stderr, "unable to create thread");
    return 1;
  }

  write_block(fds[1], &md, sizeof(md));
  close(fds[1]);

  pthread_join(th2, NULL);
  pthread_join(th1, NULL);

  fprintf(stderr, "created %s\n", path);

  // finally create the psvinf
  if (is_backup(md.name)) {
    snprintf(path, sizeof(path), "%s/%s.psvinf", outputdir, md.name);
    mfd = open(path, O_BINARY | O_WRONLY | O_CREAT | O_TRUNC, 0644);
    write_block(mfd, md.name, strnlen(md.name, 64) + 1);
    close(mfd);
    fprintf(stderr, "created %s\n", path);
  }

  return 0;
}
