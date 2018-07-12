#include "pkg.h"

#include "aes.h"
#include "pkg_out.h"
#include "pkg_utils.h"
#include "pkg_zrif.h"
#include "pkg_sys.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stddef.h>


#define PKG_HEADER_SIZE 192
#define PKG_HEADER_EXT_SIZE 64
#define ZIP_MAX_FILENAME 1024

// https://wiki.henkaku.xyz/vita/Packages#AES_Keys
static const uint8_t pkg_vita_2[] = { 0xe3, 0x1a, 0x70, 0xc9, 0xce, 0x1d, 0xd7, 0x2b, 0xf3, 0xc0, 0x62, 0x29, 0x63, 0xf2, 0xec, 0xcb };
static const uint8_t pkg_vita_3[] = { 0x42, 0x3a, 0xca, 0x3a, 0x2b, 0xd5, 0x64, 0x9f, 0x96, 0x86, 0xab, 0xad, 0x6f, 0xd8, 0x80, 0x1f };
static const uint8_t pkg_vita_4[] = { 0xaf, 0x07, 0xfd, 0x59, 0x65, 0x25, 0x27, 0xba, 0xf1, 0x33, 0x89, 0x66, 0x8b, 0x17, 0xd9, 0xea };

static pkg_output_func _output_func = sys_output;
static pkg_error_func _error_func = sys_error;
static pkg_output_progress_init_func _output_progress_init_func = sys_output_progress_init;
static pkg_output_progress_func _output_progress_func = sys_output_progress;


// http://vitadevwiki.com/vita/System_File_Object_(SFO)_(PSF)#Internal_Structure
// https://github.com/TheOfficialFloW/VitaShell/blob/1.74/sfo.h#L29
static int parse_sfo_content(const uint8_t* sfo, uint32_t sfo_size, char* category, char* title, char* content, char* min_version, char* pkg_version)
{
    if (get32le(sfo) != 0x46535000)
    {
        _error_func("ERROR: incorrect sfo signature\n");
        return -1;
    }

    uint32_t keys = get32le(sfo + 8);
    uint32_t values = get32le(sfo + 12);
    uint32_t count = get32le(sfo + 16);

    int title_index = -1;
    int content_index = -1;
    int category_index = -1;
    int minver_index = -1;
    int pkgver_index = -1;
    for (uint32_t i = 0; i < count; i++)
    {
        if (i * 16 + 20 + 2 > sfo_size)
        {
            _error_func("ERROR: sfo information is too small\n");
            return -1;
        }

        char* key = (char*)sfo + keys + get16le(sfo + i * 16 + 20);
        if (strcmp(key, "TITLE") == 0)
        {
            if (title_index < 0)
            {
                title_index = (int)i;
            }
        }
        else if (strcmp(key, "STITLE") == 0)
        {
            title_index = (int)i;
        }
        else if (strcmp(key, "CONTENT_ID") == 0)
        {
            content_index = (int)i;
        }
        else if (strcmp(key, "CATEGORY") == 0)
        {
            category_index = (int)i;
        }
        else if (strcmp(key, "PSP2_DISP_VER") == 0)
        {
            minver_index = (int)i;
        }
        else if (strcmp(key, "APP_VER") == 0)
        {
            pkgver_index = (int)i;
        }
    }

    if (title_index < 0)
    {
        _error_func("ERROR: cannot find title from sfo file, pkg is probably corrupted\n");
        return -1;
    }

    char* value = (char*)sfo + values + get32le(sfo + title_index * 16 + 20 + 12);
    size_t i;
    size_t max = 255;
    for (i = 0; i<max && *value; i++, value++)
    {
        if ((*value >= 32 && *value < 127 && strchr("<>\"/\\|?*", *value) == NULL) || (uint8_t)*value >= 128)
        {
            if (*value == ':')
            {
                *title++ = ' ';
                *title++ = '-';
                max--;
            }
            else
            {
                *title++ = *value;
            }
        }
        else if (*value == 10)
        {
            *title++ = ' ';
        }
    }
    *title = 0;

    if (content_index >= 0 && content)
    {
        value = (char*)sfo + values + get32le(sfo + content_index * 16 + 20 + 12);
        while (*value)
        {
            *content++ = *value++;
        }
        *content = 0;
    }

    if (category_index >= 0)
    {
        value = (char*)sfo + values + get32le(sfo + category_index * 16 + 20 + 12);
        while (*value)
        {
            *category++ = *value++;
        }
    }
    *category = 0;

    if (minver_index >= 0 && min_version)
    {
        value = (char*)sfo + values + get32le(sfo + minver_index * 16 + 20 + 12);
        if (*value == '0')
        {
            value++;
        }
        while (*value)
        {
            *min_version++ = *value++;
        }
        if (min_version[-1] == '0')
        {
            min_version[-1] = 0;
        }
        else
        {
            *min_version = 0;
        }
    }

    if (pkgver_index >= 0 && pkg_version)
    {
        value = (char*)sfo + values + get32le(sfo + pkgver_index * 16 + 20 + 12);
        if (*value == '0')
        {
            value++;
        }
        while (*value)
        {
            *pkg_version++ = *value++;
        }
        *pkg_version = 0;
    }
    return 0;
}

static int parse_sfo(sys_file f, uint64_t sfo_offset, uint32_t sfo_size, char* category, char* title, char* content, char* min_version, char* pkg_version)
{
    uint8_t sfo[16 * 1024];
    if (sfo_size < 16)
    {
        _error_func("ERROR: sfo information is too small\n");
        return -1;
    }
    if (sfo_size > sizeof(sfo))
    {
        _error_func("ERROR: sfo information is too big, pkg file is probably corrupted\n");
        return -1;
    }
    sys_read(f, sfo_offset, sfo, sfo_size);

    return parse_sfo_content(sfo, sfo_size, category, title, content, min_version, pkg_version);
}

static const char* get_region(const char* id)
{
    if (memcmp(id, "PCSE", 4) == 0 || memcmp(id, "PCSA", 4) == 0 ||
        memcmp(id, "NPNA", 4) == 0)
    {
        return "USA";
    }
    else if (memcmp(id, "PCSF", 4) == 0 || memcmp(id, "PCSB", 4) == 0 ||
             memcmp(id, "NPOA", 4) == 0)
    {
        return "EUR";
    }
    else if (memcmp(id, "PCSC", 4) == 0 || memcmp(id, "VCJS", 4) == 0 || 
             memcmp(id, "PCSG", 4) == 0 || memcmp(id, "VLJS", 4) == 0 ||
             memcmp(id, "VLJM", 4) == 0 || memcmp(id, "NPPA", 4) == 0)
    {
        return "JPN";
    }
    else if (memcmp(id, "VCAS", 4) == 0 || memcmp(id, "PCSH", 4) == 0 ||
             memcmp(id, "VLAS", 4) == 0 || memcmp(id, "PCSD", 4) == 0 ||
             memcmp(id, "NPQA", 4) == 0)
    {
        return "ASA";
    }
    else
    {
        return "unknown region";
    }
}


typedef enum {
    PKG_TYPE_VITA_APP,
    PKG_TYPE_VITA_DLC,
    PKG_TYPE_VITA_PATCH,
    PKG_TYPE_VITA_PSM,
} pkg_type;

static int use_sys_output = 1;

int pkg_dec(const char *pkgname, const char *target_dir, const char *zrif)
{
    if (use_sys_output) sys_output_init();
    _output_func("[*] loading...\n");

    uint64_t pkg_size;
    sys_file pkg = sys_open(pkgname, &pkg_size);

    uint8_t pkg_header[PKG_HEADER_SIZE + PKG_HEADER_EXT_SIZE];
    sys_read(pkg, 0, pkg_header, sizeof(pkg_header));

    if (get32be(pkg_header) != 0x7f504b47 || get32be(pkg_header + PKG_HEADER_SIZE) != 0x7F657874)
    {
        _error_func("ERROR: not a pkg file\n");
        return -1;
    }

    // http://www.psdevwiki.com/ps3/PKG_files
    uint64_t meta_offset = get32be(pkg_header + 8);
    uint32_t meta_count = get32be(pkg_header + 12);
    uint32_t item_count = get32be(pkg_header + 20);
    uint64_t total_size = get64be(pkg_header + 24);
    uint64_t enc_offset = get64be(pkg_header + 32);
    uint64_t enc_size = get64be(pkg_header + 40);
    const uint8_t* iv = pkg_header + 0x70;
    int key_type = pkg_header[0xe7] & 7;

    if (pkg_size < total_size)
    {
        _error_func("ERROR: pkg file is too small\n");
        return -1;
    }
    if (pkg_size < enc_offset + item_count * 32)
    {
        _error_func("ERROR: pkg file is too small\n");
        return -1;
    }

    uint32_t content_type = 0;
    uint32_t sfo_offset = 0;
    uint32_t sfo_size = 0;
    uint32_t items_offset = 0;
    uint32_t items_size = 0;

    for (uint32_t i = 0; i < meta_count; i++)
    {
        uint8_t block[16];
        sys_read(pkg, meta_offset, block, sizeof(block));

        uint32_t type = get32be(block + 0);
        uint32_t size = get32be(block + 4);

        if (type == 2)
        {
            content_type = get32be(block + 8);
        }
        else if (type == 13)
        {
            items_offset = get32be(block + 8);
            items_size = get32be(block + 12);
        }
        else if (type == 14)
        {
            sfo_offset = get32be(block + 8);
            sfo_size = get32be(block + 12);
        }

        meta_offset += 2 * sizeof(uint32_t) + size;
    }

    pkg_type type;

    if (content_type == 0x15)
    {
        type = PKG_TYPE_VITA_APP;
    }
    else if (content_type == 0x16)
    {
        type = PKG_TYPE_VITA_DLC;
    }
    else if (content_type == 0x18 || content_type == 0x1d)
    {
        type = PKG_TYPE_VITA_PSM;
    }
    else
    {
        _error_func("ERROR: unsupported content type 0x%x", content_type);
    }

    uint8_t main_key[16];
    if (key_type == 2)
    {
        aes_context key;
        aes_init(&key, pkg_vita_2, 128);
        aes_ecb_encrypt(&key, iv, main_key);
    }
    else if (key_type == 3)
    {
        aes_context key;
        aes_init(&key, pkg_vita_3, 128);
        aes_ecb_encrypt(&key, iv, main_key);
    }
    else if (key_type == 4)
    {
        aes_context key;
        aes_init(&key, pkg_vita_4, 128);
        aes_ecb_encrypt(&key, iv, main_key);
    }

    aes_context key;
    aes_init(&key, main_key, 128);

    char content[256];
    char title[256];
    char category[256];
    char min_version[256];
    char pkg_version[256];
    const char* id = content + 7;
    const char* id2 = id + 13;

    // first 512 - for vita games - https://github.com/TheOfficialFloW/NoNpDrm/blob/v1.1/src/main.c#L42
    // 1024 is used for PSM
    uint8_t rif[1024];
    uint32_t rif_size = 0;

    if (type == PKG_TYPE_VITA_PSM)
    {
        memcpy(content, pkg_header + 0x30, 0x30);
        rif_size = 1024;
    }
    else // Vita APP, DLC, PATCH or THEME
    {
        if (parse_sfo(pkg, sfo_offset, sfo_size, category, title, content, min_version, pkg_version) < 0)
            return -1;
        rif_size = 512;
            
        if (type == PKG_TYPE_VITA_APP && strcmp(category, "gp") == 0)
        {
            type = PKG_TYPE_VITA_PATCH;
        }
    }

    if (type != PKG_TYPE_VITA_PATCH && zrif != NULL)
    {
        zrif_decode(zrif, rif, rif_size);
        const char* rif_contentid = (char*)rif + (type == PKG_TYPE_VITA_PSM ? 0x50 : 0x10);
        if (strncmp(rif_contentid, content, 0x30) != 0)
        {
            _error_func("ERROR: zRIF content id '%s' doesn't match pkg '%s'\n", rif_contentid, content);
        }
    }

    char root[1024];
    if (type == PKG_TYPE_VITA_DLC)
    {
        snprintf(root, sizeof(root), "%s [%.9s] [%s] [DLC-%s]", title, id, get_region(id), id2);
        _output_func("[*] unpacking Vita DLC\n");
    }
    else if (type == PKG_TYPE_VITA_PATCH)
    {
        snprintf(root, sizeof(root), "%s [%.9s] [%s] [PATCH] [v%s]", title, id, get_region(id), pkg_version);
        _output_func("[*] unpacking Vita PATCH\n");
    }
    else if (type == PKG_TYPE_VITA_PSM)
    {
        snprintf(root, sizeof(root), "%.9s [%s] [PSM]", id, get_region(id));
        _output_func("[*] unpacking Vita PSM\n");
    }
    else if (type == PKG_TYPE_VITA_APP)
    {
        snprintf(root, sizeof(root), "%s [%.9s] [%s]", title, id, get_region(id));
        _output_func("[*] unpacking Vita APP\n");
    }
    else
    {
        assert(0);
        _error_func("ERROR: unsupported type\n");
    }

    if (target_dir == NULL)
        root[0] = 0;
    else
        strcpy(root, target_dir);

    if (type == PKG_TYPE_VITA_DLC)
    {
        sys_vstrncat(root, sizeof(root), "addcont");
        out_add_folder(root);

        sys_vstrncat(root, sizeof(root), "/%.9s", id);
        out_add_folder(root);

        sys_vstrncat(root, sizeof(root), "/%s", id2);
        out_add_folder(root);
    }
    else if (type == PKG_TYPE_VITA_PATCH)
    {
        sys_vstrncat(root, sizeof(root), "patch");
        out_add_folder(root);

        sys_vstrncat(root, sizeof(root), "/%.9s", id);
        out_add_folder(root);
    }
    else if (type == PKG_TYPE_VITA_PSM)
    {
        sys_vstrncat(root, sizeof(root), "psm");
        out_add_folder(root);

        sys_vstrncat(root, sizeof(root), "/%.9s", id);
        out_add_folder(root);
    }
    else if (type == PKG_TYPE_VITA_APP)
    {
        sys_vstrncat(root, sizeof(root), "app");
        out_add_folder(root);

        sys_vstrncat(root, sizeof(root), "/%.9s", id);
        out_add_folder(root);
    }
    else
    {
        assert(0);
        _error_func("ERROR: unsupported type\n");
    }

    char path[1024];

    int sce_sys_package_created = 0;

    _output_progress_init_func(pkg_size);

    for (uint32_t item_index = 0; item_index < item_count; item_index++)
    {
        uint8_t item[32];
        uint64_t item_offset = items_offset + item_index * 32;
        sys_read(pkg, enc_offset + item_offset, item, sizeof(item));
        aes_ctr_xor(&key, iv, item_offset / 16, item, sizeof(item));

        uint32_t name_offset = get32be(item + 0);
        uint32_t name_size = get32be(item + 4);
        uint64_t data_offset = get64be(item + 8);
        uint64_t data_size = get64be(item + 16);
        uint8_t psp_type = item[24];
        uint8_t flags = item[27];

        assert(name_offset % 16 == 0);
        assert(data_offset % 16 == 0);

        if (pkg_size < enc_offset + name_offset + name_size ||
            pkg_size < enc_offset + data_offset + data_size)
        {
            _error_func("ERROR: pkg file is too short, possibly corrupted\n");
        }

        if (name_size >= ZIP_MAX_FILENAME)
        {
            _error_func("ERROR: pkg file contains file with very long name\n");
        }

        const aes_context* item_key = &key;

        char name[ZIP_MAX_FILENAME];
        sys_read(pkg, enc_offset + name_offset, name, name_size);
        aes_ctr_xor(item_key, iv, name_offset / 16, (uint8_t*)name, name_size);
        name[name_size] = 0;

        // _output_func("[%u/%u] %s\n", item_index + 1, item_count, name);

        if (flags == 4 || flags == 18)
        {
            if (type == PKG_TYPE_VITA_PSM)
            {
                // skip "content/" prefix
                char* slash = strchr(name, '/');
                if (slash != NULL)
                {
                    snprintf(path, sizeof(path), "%s/RO/%s", root, name + 8);
                    out_add_folder(path);
                }
            }
            else if (type == PKG_TYPE_VITA_APP || type == PKG_TYPE_VITA_DLC || type == PKG_TYPE_VITA_PATCH)
            {
                snprintf(path, sizeof(path), "%s/%s", root, name);
                out_add_folder(path);

                if (strcmp("sce_sys/package", name) == 0)
                {
                    sce_sys_package_created = 1;
                }
            }
        }
        else
        {
            int decrypt = 1;
            if ((type == PKG_TYPE_VITA_APP || type == PKG_TYPE_VITA_DLC || type == PKG_TYPE_VITA_PATCH) && strcmp("sce_sys/package/digs.bin", name) == 0)
            {
                // TODO: is this really needed?
                if (!sce_sys_package_created)
                {
                    snprintf(path, sizeof(path), "%s/sce_sys/package", root);
                    out_add_folder(path);

                    sce_sys_package_created = 1;
                }
                snprintf(name, sizeof(name), "%s", "sce_sys/package/body.bin");
                decrypt = 0;
            }

            if (type == PKG_TYPE_VITA_PSM)
            {
                // skip "content/" prefix
                snprintf(path, sizeof(path), "%s/RO/%s", root, name + 8);
            }
            else
            {
                snprintf(path, sizeof(path), "%s/%s", root, name);
            }

            uint64_t offset = data_offset;

            out_begin_file(path);
            while (data_size != 0)
            {
                uint8_t PKG_ALIGN(16) buffer[1 << 16];
                uint32_t size = (uint32_t)min64(data_size, sizeof(buffer));
                _output_progress_func(enc_offset + offset);
                sys_read(pkg, enc_offset + offset, buffer, size);

                if (decrypt)
                {
                    aes_ctr_xor(item_key, iv, offset / 16, buffer, size);
                }

                out_write(buffer, size);
                offset += size;
                data_size -= size;
            }
            out_end_file();
        }
    }

    _output_func("[*] unpacking completed\n");

    if (type == PKG_TYPE_VITA_APP || type == PKG_TYPE_VITA_DLC || type == PKG_TYPE_VITA_PATCH)
    {
        if (!sce_sys_package_created)
        {
            _output_func("[*] creating sce_sys/package\n");
            snprintf(path, sizeof(path), "%s/sce_sys/package", root);
            out_add_folder(path);
        }

        _output_func("[*] creating sce_sys/package/head.bin\n");
        snprintf(path, sizeof(path), "%s/sce_sys/package/head.bin", root);

        out_begin_file(path);
        uint64_t head_size = enc_offset + items_size;
        uint64_t head_offset = 0;
        while (head_size != 0)
        {
            uint8_t PKG_ALIGN(16) buffer[1 << 16];
            uint32_t size = (uint32_t)min64(head_size, sizeof(buffer));
            sys_read(pkg, head_offset, buffer, size);
            out_write(buffer, size);
            head_size -= size;
            head_offset += size;
        }
        out_end_file();

        _output_func("[*] creating sce_sys/package/tail.bin\n");
        snprintf(path, sizeof(path), "%s/sce_sys/package/tail.bin", root);

        out_begin_file(path);
        uint64_t tail_offset = enc_offset + enc_size;
        while (tail_offset != pkg_size)
        {
            uint8_t PKG_ALIGN(16) buffer[1 << 16];
            uint32_t size = (uint32_t)min64(pkg_size - tail_offset, sizeof(buffer));
            sys_read(pkg, tail_offset, buffer, size);
            out_write(buffer, size);
            tail_offset += size;
        }
        out_end_file();

        _output_func("[*] creating sce_sys/package/stat.bin\n");
        snprintf(path, sizeof(path), "%s/sce_sys/package/stat.bin", root);

        uint8_t stat[768] = { 0 };
        out_begin_file(path);
        out_write(stat, sizeof(stat));
        out_end_file();
    }

    if ((type == PKG_TYPE_VITA_APP || type == PKG_TYPE_VITA_DLC || type == PKG_TYPE_VITA_PSM) && zrif != NULL)
    {
        if (type == PKG_TYPE_VITA_PSM)
        {
            _output_func("[*] creating RO/License\n");
            snprintf(path, sizeof(path), "%s/RO/License", root);
            out_add_folder(path);

            _output_func("[*] creating RO/License/FAKE.rif\n");
            snprintf(path, sizeof(path), "%s/RO/License/FAKE.rif", root);
        }
        else
        {
            _output_func("[*] creating sce_sys/package/work.bin\n");
            snprintf(path, sizeof(path), "%s/sce_sys/package/work.bin", root);
        }

        out_begin_file(path);
        out_write(rif, rif_size);
        out_end_file();
    }

    if (type == PKG_TYPE_VITA_PSM)
    {
        _output_func("[*] creating RW\n");
        snprintf(path, sizeof(path), "%s/RW", root);
        out_add_folder(path);

        _output_func("[*] creating RW/Documents\n");
        snprintf(path, sizeof(path), "%s/RW/Documents", root);
        out_add_folder(path);

        _output_func("[*] creating RW/Temp\n");
        snprintf(path, sizeof(path), "%s/RW/Temp", root);
        out_add_folder(path);

        _output_func("[*] creating RW/System\n");
        snprintf(path, sizeof(path), "%s/RW/System", root);
        out_add_folder(path);

        _output_func("[*] creating RW/System/content_id\n");
        snprintf(path, sizeof(path), "%s/RW/System/content_id", root);
        out_begin_file(path);
        out_write(pkg_header + 0x30, 0x30);
        out_end_file();

        _output_func("[*] creating RW/System/pm.dat\n");
        snprintf(path, sizeof(path), "%s/RW/System/pm.dat", root);

        uint8_t pm[1 << 16] = { 0 };
        out_begin_file(path);
        out_write(pm, sizeof(pm));
        out_end_file();
    }

    if (type == PKG_TYPE_VITA_APP || type == PKG_TYPE_VITA_PATCH)
    {
        _output_func("[*] minimum fw version required: %s\n", min_version);
    }

    _output_func("[*] done!\n");
    if (use_sys_output) sys_output_done();
    return 0;
}

void dummy_out(const char* msg, ...) {}
void dummy_error(const char* msg, ...) {}
void dummy_progress_init(uint64_t size) {}
void dummy_progress(uint64_t progress) {}

void pkg_disable_output() {
    pkg_set_func(dummy_out, dummy_error, dummy_progress_init, dummy_progress);
}

void pkg_set_func(pkg_output_func out, pkg_error_func err,
    pkg_output_progress_init_func proginit,
    pkg_output_progress_func prog) {
    use_sys_output = 0;
    if (out != NULL)
        _output_func = out;
    else {
        _output_func = sys_output;
        use_sys_output = 1;
    }
    if (err != NULL)
        _error_func = err;
    else {
        _error_func = sys_error;
        use_sys_output = 1;
    }
    if (proginit != NULL)
        _output_progress_init_func = proginit;
    else {
        _output_progress_init_func = sys_output_progress_init;
        use_sys_output = 1;
    }
    if (prog != NULL)
        _output_progress_func = prog;
    else {
        _output_progress_func = sys_output_progress;
        use_sys_output = 1;
    }
}
