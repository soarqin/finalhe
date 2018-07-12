/* Copyright (C) 2017 Yifan Lu
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#ifdef _MSC_VER
#include "dirent_win32.h"
#include <io.h>
#else
#include <dirent.h>
#include <utime.h>
#include <unistd.h>
#endif
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <zlib.h>
#include "crypto.h"
#include "backup.h"
#include "endian-utils.h"
#include "psvimg.h"
#include "utils.h"

#define MAX_PATH_LEN 1024

void *encrypt_thread(void *pargs) {
  args_t *args = (args_t *)pargs;
  sha256_context ctx, tmp;
  uint8_t iv[AES_BLOCK_SIZE];
  uint8_t buffer[PSVIMG_BLOCK_SIZE + SHA256_BLOCK_SIZE + 0x10];
  ssize_t rd;
  union {
#pragma pack(push, 1)
    struct {
      uint32_t padding;
      uint32_t unused;
      uint64_t total;
    } ATTR_PACKED data;
#pragma pack(pop)
    uint8_t raw[AES_BLOCK_SIZE];
  } footer;

  // write iv
  memcpy(iv, args->iv, AES_BLOCK_SIZE);
  aes256_encrypt(iv, args->key);
  write_block(args->out, iv, AES_BLOCK_SIZE);

  // encrypt blocks
  sha256_init(&ctx);
  sha256_starts(&ctx);
  footer.data.total = AES_BLOCK_SIZE; // from iv
  footer.data.padding = 0;
  footer.data.unused = 0;
  while ((rd = read_block(args->in, buffer, PSVIMG_BLOCK_SIZE)) > 0) {
    // generate hash
    sha256_update(&ctx, buffer, rd);
    sha256_copy(&tmp, &ctx);
    sha256_final(&tmp, &buffer[rd]);
    rd += SHA256_BLOCK_SIZE;

    // add padding
    if (footer.data.padding != 0) { // we should be reading 0x8000 blocks! only need padding once
      fprintf(stderr, "an internal error has occured!\n");
      goto end;
    }
    if (rd & (AES_BLOCK_SIZE-1)) {
      footer.data.padding = AES_BLOCK_SIZE - (rd & (AES_BLOCK_SIZE-1));
      rd += footer.data.padding;
    }

    // encrypt
    aes256_cbc_encrypt(buffer, args->key, iv, rd / AES_BLOCK_SIZE);

    // save next iv
    memcpy(iv, &buffer[rd - AES_BLOCK_SIZE], AES_BLOCK_SIZE);

    // write output
    write_block(args->out, buffer, rd);

    footer.data.total += rd;
  }

  if (rd < 0) {
    fprintf(stderr, "Read error occured!\n");
    goto end;
  }

  // send footer
  footer.data.total += 0x10;
  footer.data.padding = htole32(footer.data.padding);
  footer.data.total = htole64(footer.data.total);
  aes256_cbc_encrypt(footer.raw, args->key, iv, 1);
  write_block(args->out, footer.raw, AES_BLOCK_SIZE);

end:
  return NULL;
}

void *compress_thread(void *pargs) {
  args_t *args = (args_t *)pargs;

  int ret;
  unsigned have;
  z_stream strm;
  unsigned char in[PSVIMG_BLOCK_SIZE];
  unsigned char out[PSVIMG_BLOCK_SIZE];
  ssize_t rd;

  /* allocate deflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
  if (ret != Z_OK) {
    fprintf(stderr, "error init zlib\n");
    goto end;
  }

  /* compress until end of file */
  do {
    strm.avail_in = rd = read_block(args->in, in, sizeof(in));
    if (rd < 0) {
      fprintf(stderr, "error reading\n");
      goto end;
    }
    strm.next_in = in;

    /* run deflate() on input until output buffer not full, finish
       compression if all of source has been read in */
    do {
      strm.avail_out = PSVIMG_BLOCK_SIZE;
      strm.next_out = out;
      ret = deflate(&strm, Z_NO_FLUSH);    /* no bad return value */
      if (ret == Z_STREAM_ERROR) {  /* state not clobbered */
        fprintf(stderr, "zlib internal error\n");
        goto end;
      }
      have = PSVIMG_BLOCK_SIZE - strm.avail_out;
      if (write_block(args->out, out, have) < have) {
        fprintf(stderr, "error writing\n");
        goto end;
      }
    } while (strm.avail_out == 0);
    if (strm.avail_in != 0){     /* all input will be used */
      fprintf(stderr, "zlib internal error\n");
      goto end;
    }

    /* done when last data in file processed */
  } while (rd > 0);
  if (deflate(&strm, Z_FINISH) == Z_STREAM_ERROR) {  /* state not clobbered */
    fprintf(stderr, "zlib internal error\n");
    goto end;
  }
  have = PSVIMG_BLOCK_SIZE - strm.avail_out;
  if (write_block(args->out, out, have) < have) {
    fprintf(stderr, "error writing\n");
    goto end;
  }

  /* clean up and return */
  (void)deflateEnd(&strm);

end:
  return NULL;
}

static void time_to_scetime(const time_t *time, SceDateTime *sce) {
  struct tm *tmp;
  tmp = localtime(time);
  sce->second = htole32(tmp->tm_sec);
  sce->minute = htole32(tmp->tm_min);
  sce->hour = htole32(tmp->tm_hour);
  sce->day = htole32(tmp->tm_mday);
  sce->month = htole32(tmp->tm_mon + 1);
  sce->year = htole32(tmp->tm_year + 1900);
  sce->microsecond = htole32(0);
}

static int scestat(const char *path, SceIoStat *sce) {
  struct stat st;

  if (stat(path, &st) < 0) {
    return -1;
  }

  sce->sst_mode = 0;
  if (S_ISDIR(st.st_mode)) {
    sce->sst_mode |= SCE_S_IFDIR;
  } else {
    sce->sst_mode |= SCE_S_IFREG;
  }
  if ((st.st_mode & S_IRUSR) == S_IRUSR) {
    sce->sst_mode |= SCE_S_IRUSR;
  }
  if ((st.st_mode & S_IWUSR) == S_IWUSR) {
    sce->sst_mode |= SCE_S_IWUSR;
  }
  if ((st.st_mode & S_IRGRP) == S_IRGRP) {
    sce->sst_mode |= SCE_S_IRGRP;
  }
  if ((st.st_mode & S_IWGRP) == S_IWGRP) {
    sce->sst_mode |= SCE_S_IWGRP;
  }
  if ((st.st_mode & S_IROTH) == S_IROTH) {
    sce->sst_mode |= SCE_S_IROTH;
  }
  if ((st.st_mode & S_IWOTH) == S_IWOTH) {
    sce->sst_mode |= SCE_S_IWOTH;
  }
  sce->sst_mode = htole32(sce->sst_mode);
  sce->sst_attr = htole32(0);
  sce->sst_size = htole64(st.st_size);
  time_to_scetime(&st.st_ctime, &sce->sst_ctime);
  time_to_scetime(&st.st_atime, &sce->sst_atime);
  time_to_scetime(&st.st_mtime, &sce->sst_mtime);
  memset(sce->sst_private, 0, sizeof(sce->sst_private));

  return 0;
}

static ssize_t add_all_files(int fd, const char *parent, const char *rel, const char *host);

static ssize_t add_file(int fd, const char *parent, const char *rel, const char *host) {
  PsvImgHeader_t header;
  PsvImgTailer_t tailer;
  uint64_t fsize;
  int file;
  int padding;

  // create header
  memset(&header, PSVIMG_HEADER_FILLER, sizeof(header));
  header.systime = htole64(0);
  header.flags = htole64(0);
  if (scestat(host, &header.stat) < 0) {
    fprintf(stderr, "error getting stat for %s\n", host);
    return -1;
  }
  strncpy(header.path_parent, parent, sizeof(header.path_parent));
  header.unk_16C = htole32(1);
  strncpy(header.path_rel, rel, sizeof(header.path_rel));
  memcpy(header.end, PSVIMG_ENDOFHEADER, sizeof(header.end));

  // send header
  write_block(fd, &header, sizeof(header));

  // send file
  if (SCE_S_ISREG(le32toh(header.stat.sst_mode))) {
    fsize = le64toh(header.stat.sst_size);
    printf("packing file %s%s (%llx bytes)...\n", header.path_parent, header.path_rel, fsize);
    file = open(host, O_RDONLY | O_BINARY);
    if (file < 0) {
      fprintf(stderr, "error opening %s\n", host);
      return -1;
    }
    if (copy_block(fd, file, fsize) < fsize) {
      fprintf(stderr, "error writing file data\n");
      close(file);
      return -1;
    }
    close(file);
  } else {
    fsize = 0;
  }

  // send padding
  if (fsize & (PSVIMG_ENTRY_ALIGN-1)) {
    padding = PSVIMG_ENTRY_ALIGN - (fsize & (PSVIMG_ENTRY_ALIGN-1));
  } else {
    padding = 0;
  }
  while (padding --> 0) {
    char ch = (padding == 0) ? '\n' : PSVIMG_PADDING_FILLER;
    write_block(fd, &ch, 1);
  }

  // send tailer
  memset(&tailer, PSVIMG_TAILER_FILLER, sizeof(tailer));
  tailer.flags = htole64(0);
  memcpy(tailer.end, PSVIMG_ENDOFTAILER, sizeof(tailer.end));
  write_block(fd, &tailer, sizeof(tailer));

  if (SCE_S_ISDIR(le32toh(header.stat.sst_mode))) {
    printf("packing directory %s%s...\n", header.path_parent, header.path_rel);
    return add_all_files(fd, parent, rel, host);
  } else {
    return (ssize_t)fsize;
  }
}

static ssize_t add_all_files(int fd, const char *parent, const char *rel, const char *host) {
  char new_rel[256];
  char new_host[MAX_PATH_LEN];
  DIR *dir;
  struct dirent *dent;
  ssize_t fsize, total;

  if ((dir = opendir(host)) == NULL) {
    fprintf(stderr, "cannot open %s\n", host);
    return -1;
  }

  total = 0;
  while ((dent = readdir(dir)) != NULL) {
    if (rel[0] == '\0' && strcmp(dent->d_name, "VITA_PATH.TXT") == 0) {
      // skip this file
      continue;
    }

    if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0) {
      continue;
    }

#ifdef __APPLE__
    if (strcmp(dent->d_name, ".DS_Store") == 0) {
      continue; // ignore annoying OSX specific files
    }
#endif

    if (rel[0] == '\0' && strcmp(dent->d_name, "VITA_DATA.BIN") == 0) { // special file
      new_rel[0] = '\0';
    } else {
      if (snprintf(new_rel, sizeof(new_rel), "%s/%s", rel, dent->d_name) == sizeof(new_rel)) {
        fprintf(stderr, "path is too long! cannot add %s/%s\n", rel, dent->d_name);
        goto err;
      }
    }
    snprintf(new_host, sizeof(new_host), "%s/%s", host, dent->d_name);

    dent = NULL; // so we don't actually reuse it
    // add file/directory to psvimg
    if ((fsize = add_file(fd, parent, new_rel, new_host)) < 0) {
      goto err;
    }
    total += fsize;
  }

  closedir(dir);
  return total;

err:
  closedir(dir);
  return -1;
}

void *pack_thread(void *pargs) {
  args_t *args = (args_t *)pargs;
  DIR *dir;
  struct dirent *dent;
  struct stat st;
  char parent[256];
  char host[MAX_PATH_LEN];
  char parent_file[MAX_PATH_LEN];
  int fd;
  ssize_t rd, wr;
  
  if ((dir = opendir(args->prefix)) == NULL) {
    fprintf(stderr, "cannot open %s\n", args->prefix);
    goto end2;
  }

  while ((dent = readdir(dir)) != NULL) {
    if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0) {
      continue;
    }

#ifdef __APPLE__
    if (strcmp(dent->d_name, ".DS_Store") == 0) {
      continue; // ignore annoying OSX specific files
    }
#endif

    snprintf(host, sizeof(host), "%s/%s", args->prefix, dent->d_name);
    if (stat(host, &st) < 0) {
      fprintf(stderr, "internal error\n");
      goto end;
    }

    if (S_ISDIR(st.st_mode)) {
      snprintf(parent_file, sizeof(parent_file), "%s/%s", host, "VITA_PATH.TXT");
      if ((fd = open(parent_file, O_RDONLY | O_BINARY)) < 0) {
        fprintf(stderr, "WARNING: skipping %s because VITA_PATH.TXT is not found!\n", host);
        continue;
      }
      if ((rd = read_block(fd, parent, sizeof(parent)-1)) < 0) {
        fprintf(stderr, "error reading %s\n", parent_file);
        goto end;
      }
      close(fd);
      parent[rd] = '\0';
      dent = NULL; // so we don't actually reuse it
      printf("adding files for %s\n", parent);
      if ((wr = add_all_files(args->out, parent, "\0", host)) < 0) {
        goto end;
      }
      args->content_size += wr;
    } else {
      fprintf(stderr, "WARNING: skipping %s because it is not a directory!\n", host);
    }
  }
  
end:
  closedir(dir);
end2:
  return NULL;
}
