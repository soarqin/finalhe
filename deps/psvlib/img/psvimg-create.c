/* Copyright (C) 2017 Yifan Lu
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include "psvimg-create.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(n, m) _mkdir(n)
#else
#include <unistd.h>
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
    uint8_t iv[16], keys[32];
    struct stat st;
    char path[MAX_PATH_LEN];
    FILE* mfd;
    PsvMd_t md;
    uint64_t content_size;

    srand((uint32_t)time(NULL));
    if (is_meta) {
        mfd = fopen(meta_name, "rb");
        if (mfd == NULL) {
            fprintf(stderr, "metadata");
            return 1;
        }
        if (fread(&md, 1, sizeof(md) - sizeof(md.add_data), mfd) < sizeof(md) - sizeof(md.add_data)) {
            fprintf(stderr, "invalid metadata size\n");
            return 1;
        }
        if (md.type != 2) {
            fprintf(stderr, "metadata type not supported\n");
            fclose(mfd);
            return 1;
        }
        if (fread(&md.add_data, 1, sizeof(md.add_data), mfd) < sizeof(md.add_data)) {
            fprintf(stderr, "invalid metadata size\n");
            fclose(mfd);
            return 1;
        }
        fclose(mfd);
    } else {
        memset(&md, 0, sizeof(md));
        strncpy(md.name, meta_name, sizeof(md.name));
        md.magic = is_backup(md.name) ? PSVMD_BACKUP_MAGIC : PSVMD_CONTENT_MAGIC;
        md.type = 2;
        md.version = 2;
        md.add_data = 1;
        for (int i = 0; i < sizeof(md.iv); i++) {
            md.iv[i] = rand() % 0xFF;
        }
    }

    if (parse_key(key, keys) < 0) {
        fprintf(stderr, "invalid key\n");
        return 1;
    }

    if (stat(outputdir, &st) < 0) {
        mkdir(outputdir, 0700);
    }
    snprintf(path, sizeof(path), "%s/%s.psvimg", outputdir, md.name);
    memcpy(iv, md.iv, sizeof(iv));
    pack_and_encrypt(path, &content_size, inputdir, iv, keys);
    if (stat(path, &st) < 0) {
        fprintf(stderr, "stat");
        return 1;
    }
    md.total_size = content_size;
    md.psvimg_size = st.st_size;

    // now create the psvmd
    snprintf(path, sizeof(path), "%s/%s.psvmd", outputdir, md.name);

    for (int i = 0; i < sizeof(md.iv); i++) {
        iv[i] = rand() % 0xFF;
    }

    compress_and_encrypt(path, (const uint8_t*)&md, sizeof(md), iv, keys);

    fprintf(stderr, "created %s\n", path);

    // finally create the psvinf
    if (is_backup(md.name)) {
        snprintf(path, sizeof(path), "%s/%s.psvinf", outputdir, md.name);
        mfd = fopen(path, "wb");
        fwrite(md.name, 1, strnlen(md.name, 64) + 1, mfd);
        fclose(mfd);
        fprintf(stderr, "created %s\n", path);
    }

    return 0;
}
