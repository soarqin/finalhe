/* Copyright (C) 2017 Yifan Lu0
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
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

typedef struct process_context_t {
    sha256_context ctx;
    aes_context aes;
    uint8_t iv[AES_BLOCK_SIZE];
    uint8_t buffer[PSVIMG_BLOCK_SIZE + SHA256_BLOCK_SIZE + 0x10];
    size_t buffer_size;
    FILE* out;

    union {
#pragma pack(push, 1)
        struct {
            uint32_t padding;
            uint32_t unused;
            uint64_t total;
        } ATTR_PACKED;
#pragma pack(pop)
        uint8_t raw[AES_BLOCK_SIZE];
    } footer;
} process_context_t;


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

int encrypt_init(process_context_t* cont, const char* out_file, uint8_t iv[AES_BLOCK_SIZE], uint8_t key[32]) {
    cont->out = fopen(out_file, "wb");
    if (!cont->out) return -1;

    memset(cont->buffer, 0, sizeof(cont->buffer));
    memcpy(cont->iv, iv, AES_BLOCK_SIZE);
    aes_init(&cont->aes, key, 256);
    aes_ecb_encrypt(&cont->aes, cont->iv, cont->iv);
    fwrite(cont->iv, 1, AES_BLOCK_SIZE, cont->out);

    sha256_init(&cont->ctx);
    sha256_starts(&cont->ctx);

    cont->buffer_size = 0;

    cont->footer.total = AES_BLOCK_SIZE; // from iv
    cont->footer.padding = 0;
    cont->footer.unused = 0;
    return 0;
}

void encrypt_data(process_context_t* cont, const uint8_t* buffer, size_t size) {
    while (cont->buffer_size + size >= PSVIMG_BLOCK_SIZE) {
        sha256_context tmp;

        size_t copy_size = PSVIMG_BLOCK_SIZE - cont->buffer_size;
        memcpy(cont->buffer + cont->buffer_size, buffer, copy_size);
        buffer += copy_size;
        size -= copy_size;

        // generate hash
        sha256_update(&cont->ctx, cont->buffer, PSVIMG_BLOCK_SIZE);
        sha256_copy(&tmp, &cont->ctx);
        sha256_final(&tmp, cont->buffer + PSVIMG_BLOCK_SIZE);
        aes_cbc_encrypt(&cont->aes, cont->iv, cont->buffer, PSVIMG_BLOCK_SIZE + SHA256_BLOCK_SIZE, cont->buffer);
        fwrite(cont->buffer, 1, PSVIMG_BLOCK_SIZE + SHA256_BLOCK_SIZE, cont->out);
        cont->footer.total += PSVIMG_BLOCK_SIZE + SHA256_BLOCK_SIZE;

        cont->buffer_size = 0;
    }
    if (size > 0) {
        memcpy(cont->buffer + cont->buffer_size, buffer, size);
        cont->buffer_size += size;
    }
}

void encrypt_finish(process_context_t* cont) {
    sha256_update(&cont->ctx, cont->buffer, cont->buffer_size);
    sha256_final(&cont->ctx, cont->buffer + cont->buffer_size);
    cont->buffer_size += SHA256_BLOCK_SIZE;
    if (cont->buffer_size & (AES_BLOCK_SIZE - 1)) {
        cont->footer.padding = AES_BLOCK_SIZE - (cont->buffer_size & (AES_BLOCK_SIZE - 1));
        cont->buffer_size += cont->footer.padding;
    }
    aes_cbc_encrypt(&cont->aes, cont->iv, cont->buffer, cont->buffer_size, cont->buffer);
    fwrite(cont->buffer, 1, cont->buffer_size, cont->out);
    cont->footer.total += cont->buffer_size;
    cont->buffer_size = 0;

    cont->footer.total += 0x10;
    cont->footer.padding = htole32(cont->footer.padding);
    cont->footer.total = htole64(cont->footer.total);
    aes_cbc_encrypt(&cont->aes, cont->iv, cont->footer.raw, AES_BLOCK_SIZE, cont->footer.raw);
    fwrite(cont->footer.raw, 1, AES_BLOCK_SIZE, cont->out);

    fclose(cont->out);
}

int compress_and_encrypt(const char* output, const uint8_t* data, size_t data_size, uint8_t iv[16], uint8_t key[32]) {
    process_context_t cont;

    int ret;
    uint32_t have;
    z_stream strm;
    uint8_t in[PSVIMG_BLOCK_SIZE];
    uint8_t out[PSVIMG_BLOCK_SIZE];

    if (encrypt_init(&cont, output, iv, key) < 0) {
        return -1;
    }

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
        encrypt_finish(&cont);
        return -1;
    }

    /* compress until end of file */
    while (data_size > 0) {
        if (data_size > sizeof(in)) {
            strm.avail_in = sizeof(in);
            strm.next_in = (Bytef*)data;
            data += sizeof(in);
            data_size -= sizeof(in);
        } else {
            strm.avail_in = data_size;
            strm.next_in = (Bytef*)data;
            data += data_size;
            data_size = 0;
        }

        do {
            strm.avail_out = sizeof(out);
            strm.next_out = out;
            ret = deflate(&strm, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR) {
                deflateEnd(&strm);
                encrypt_finish(&cont);
                return -1;
            }
            have = sizeof(out) - strm.avail_out;
            encrypt_data(&cont, out, have);
        } while (strm.avail_out == 0);
        if (strm.avail_in != 0) {
            deflateEnd(&strm);
            encrypt_finish(&cont);
            return -1;
        }

        /* done when last data in file processed */
    }
    strm.avail_out = sizeof(out);
    strm.next_out = out;
    if (deflate(&strm, Z_FINISH) == Z_STREAM_ERROR) {  /* state not clobbered */
        deflateEnd(&strm);
        encrypt_finish(&cont);
        return -1;
    }
    have = sizeof(out) - strm.avail_out;
    encrypt_data(&cont, out, have);

    deflateEnd(&strm);

    encrypt_finish(&cont);
    return 0;
}

static ssize_t pack_add_all_files(process_context_t* cont, const char *parent, const char *rel, const char *host);

static ssize_t pack_add_file(process_context_t* cont, const char *parent, const char *rel, const char *host) {
    PsvImgHeader_t header;
    PsvImgTailer_t tailer;
    uint64_t fsize;
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

    encrypt_data(cont, (const uint8_t*)&header, sizeof(header));

    // send file
    if (SCE_S_ISREG(le32toh(header.stat.sst_mode))) {
        FILE* file;
        uint8_t block[PSVIMG_BLOCK_SIZE];
        uint64_t total = 0ULL;
        fsize = le64toh(header.stat.sst_size);
        printf("packing file %s%s (%llx bytes)...\n", header.path_parent,
               header.path_rel, fsize);
        file = fopen(host, "rb");
        if (file == NULL) {
            fprintf(stderr, "error opening %s\n", host);
            return -1;
        }

        while(!feof(file)) {
            size_t sz = fread(block, 1, PSVIMG_BLOCK_SIZE, file);
            if (sz) {
                encrypt_data(cont, block, sz);
                total += sz;
            }
        }
        fclose(file);
    } else {
        fsize = 0;
    }

    // send padding
    if (fsize & (PSVIMG_ENTRY_ALIGN - 1)) {
        padding = PSVIMG_ENTRY_ALIGN - (fsize & (PSVIMG_ENTRY_ALIGN - 1));
    } else {
        padding = 0;
    }
    while (padding-- > 0) {
        char ch = (padding == 0) ? '\n' : PSVIMG_PADDING_FILLER;
        encrypt_data(cont, (const uint8_t*)&ch, 1);
    }

    // send tailer
    memset(&tailer, PSVIMG_TAILER_FILLER, sizeof(tailer));
    tailer.flags = htole64(0);
    memcpy(tailer.end, PSVIMG_ENDOFTAILER, sizeof(tailer.end));
    encrypt_data(cont, (const uint8_t*)&tailer, sizeof(tailer));

    if (SCE_S_ISDIR(le32toh(header.stat.sst_mode))) {
        printf("packing directory %s%s...\n", header.path_parent,
               header.path_rel);
        return pack_add_all_files(cont, parent, rel, host);
    } else {
        return (ssize_t)fsize;
    }
}

static ssize_t pack_add_all_files(process_context_t* cont, const char *parent, const char *rel,
                             const char *host) {
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
            continue;  // ignore annoying OSX specific files
        }
#endif

        if (rel[0] == '\0' &&
            strcmp(dent->d_name, "VITA_DATA.BIN") == 0) {  // special file
            new_rel[0] = '\0';
        } else {
            if (snprintf(new_rel, sizeof(new_rel), "%s/%s", rel,
                         dent->d_name) == sizeof(new_rel)) {
                fprintf(stderr, "path is too long! cannot add %s/%s\n", rel,
                        dent->d_name);
                goto err;
            }
        }
        snprintf(new_host, sizeof(new_host), "%s/%s", host, dent->d_name);

        dent = NULL;  // so we don't actually reuse it
        // add file/directory to psvimg
        if ((fsize = pack_add_file(cont, parent, new_rel, new_host)) < 0) {
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

int pack_and_encrypt(const char* output, uint64_t* content_size, const char* prefix, uint8_t iv[16], uint8_t key[32]) {
    DIR *dir;
    struct dirent *dent;
    struct stat st;
    char parent[256];
    char host[MAX_PATH_LEN];
    char parent_file[MAX_PATH_LEN];
    FILE* fd;
    ssize_t rd, wr;
    process_context_t cont;

    *content_size = 0ULL;
    if ((dir = opendir(prefix)) == NULL) {
        fprintf(stderr, "cannot open %s\n", prefix);
        return -1;
    }

    encrypt_init(&cont, output, iv, key);

    while ((dent = readdir(dir)) != NULL) {
        if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0) {
            continue;
        }

#ifdef __APPLE__
        if (strcmp(dent->d_name, ".DS_Store") == 0) {
            continue; // ignore annoying OSX specific files
        }
#endif

        snprintf(host, sizeof(host), "%s/%s", prefix, dent->d_name);
        if (stat(host, &st) < 0) {
            fprintf(stderr, "internal error\n");
            goto end;
        }

        if (S_ISDIR(st.st_mode)) {
            snprintf(parent_file, sizeof(parent_file), "%s/%s", host, "VITA_PATH.TXT");
            if ((fd = fopen(parent_file, "rb")) == 0) {
                fprintf(stderr, "WARNING: skipping %s because VITA_PATH.TXT is not found!\n", host);
                continue;
            }
            rd = fread(parent, 1, sizeof(parent) - 1, fd);
            fclose(fd);
            parent[rd] = '\0';
            dent = NULL; // so we don't actually reuse it
            printf("adding files for %s\n", parent);
            if ((wr = pack_add_all_files(&cont, parent, "\0", host)) < 0) {
                goto end;
            }
            *content_size += (uint64_t)wr;
        } else {
            fprintf(stderr, "WARNING: skipping %s because it is not a directory!\n", host);
        }
    }

end:
    closedir(dir);
    encrypt_finish(&cont);

    return 0;
}
