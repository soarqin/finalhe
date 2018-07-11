#include "aes.h"

#include <string.h>
#include <wmmintrin.h> // AESNI
#include <tmmintrin.h> // SSSE3

#define AES_INIT_128(rkeys, i, rcon)               \
{                                                  \
    __m128i _s = rkeys[i];                         \
    __m128i _t = rkeys[i];                         \
    _s = _mm_xor_si128(_s, _mm_slli_si128(_s, 4)); \
    _s = _mm_xor_si128(_s, _mm_slli_si128(_s, 8)); \
    _t = _mm_aeskeygenassist_si128(_t, rcon);      \
    _t = _mm_shuffle_epi32(_t, 0xff);              \
    rkeys[i + 1] = _mm_xor_si128(_s, _t);              \
}

#define AES_INIT_256(rkeys, i, shuffle, rcon)      \
{                                                  \
    __m128i _s = rkeys[i];                         \
    __m128i _t = rkeys[i + 1];                     \
    _s = _mm_xor_si128(_s, _mm_slli_si128(_s, 4)); \
    _s = _mm_xor_si128(_s, _mm_slli_si128(_s, 8)); \
    _t = _mm_aeskeygenassist_si128(_t, rcon);      \
    _t = _mm_shuffle_epi32(_t, shuffle);           \
    rkeys[i + 2] = _mm_xor_si128(_s, _t);          \
}

void aes_init_x86(aes_context* ctx, const uint8_t* key)
{
    __m128i* ekey = (__m128i*)ctx->key;

    if (ctx->nr == 10) {
        ekey[0] = _mm_loadu_si128((const __m128i*)key);
        AES_INIT_128(ekey, 0, 0x01);
        AES_INIT_128(ekey, 1, 0x02);
        AES_INIT_128(ekey, 2, 0x04);
        AES_INIT_128(ekey, 3, 0x08);
        AES_INIT_128(ekey, 4, 0x10);
        AES_INIT_128(ekey, 5, 0x20);
        AES_INIT_128(ekey, 6, 0x40);
        AES_INIT_128(ekey, 7, 0x80);
        AES_INIT_128(ekey, 8, 0x1b);
        AES_INIT_128(ekey, 9, 0x36);
    } else {
        ekey[0] = _mm_loadu_si128((const __m128i*)key);
        ekey[1] = _mm_loadu_si128((const __m128i*)(key+16));
        AES_INIT_256(ekey, 0,  0xFF, 0x01);
        AES_INIT_256(ekey, 1,  0xAA, 0x00);
        AES_INIT_256(ekey, 2,  0xFF, 0x02);
        AES_INIT_256(ekey, 3,  0xAA, 0x00);
        AES_INIT_256(ekey, 4,  0xFF, 0x04);
        AES_INIT_256(ekey, 5,  0xAA, 0x00);
        AES_INIT_256(ekey, 6,  0xFF, 0x08);
        AES_INIT_256(ekey, 7,  0xAA, 0x00);
        AES_INIT_256(ekey, 8,  0xFF, 0x10);
        AES_INIT_256(ekey, 9,  0xAA, 0x00);
        AES_INIT_256(ekey, 10, 0xFF, 0x20);
        AES_INIT_256(ekey, 11, 0xAA, 0x00);
        AES_INIT_256(ekey, 12, 0xFF, 0x40);
    }
}

void aes_init_dec_x86(aes_context* ctx, const uint8_t* key)
{
    aes_context enc;
    enc.nr = ctx->nr;
    aes_init_x86(&enc, key);

    const __m128i* ekey = (__m128i*)&enc.key;
    __m128i* dkey = (__m128i*)&ctx->key;

    _mm_store_si128(dkey + ctx->nr, _mm_load_si128(ekey + 0));
    for (size_t i = 1; i < ctx->nr; i++)
    {
        _mm_store_si128(dkey + ctx->nr - i, _mm_aesimc_si128(_mm_load_si128(ekey + i)));
    }
    _mm_store_si128(dkey + 0, _mm_load_si128(ekey + ctx->nr));
}

static __m128i aes_encrypt_x86(__m128i input, const __m128i* key, int nr)
{
    __m128i tmp;
    tmp = _mm_xor_si128(input, _mm_load_si128(key + 0));
    tmp = _mm_aesenc_si128(tmp, _mm_load_si128(key + 1));
    tmp = _mm_aesenc_si128(tmp, _mm_load_si128(key + 2));
    tmp = _mm_aesenc_si128(tmp, _mm_load_si128(key + 3));
    tmp = _mm_aesenc_si128(tmp, _mm_load_si128(key + 4));
    tmp = _mm_aesenc_si128(tmp, _mm_load_si128(key + 5));
    tmp = _mm_aesenc_si128(tmp, _mm_load_si128(key + 6));
    tmp = _mm_aesenc_si128(tmp, _mm_load_si128(key + 7));
    tmp = _mm_aesenc_si128(tmp, _mm_load_si128(key + 8));
    tmp = _mm_aesenc_si128(tmp, _mm_load_si128(key + 9));
    if (nr > 10)
    {
        tmp = _mm_aesenc_si128(tmp, _mm_load_si128(key + 10));
        tmp = _mm_aesenc_si128(tmp, _mm_load_si128(key + 11));
        tmp = _mm_aesenc_si128(tmp, _mm_load_si128(key + 12));
        tmp = _mm_aesenc_si128(tmp, _mm_load_si128(key + 13));
    }
    return _mm_aesenclast_si128(tmp, _mm_load_si128(key + nr));
}

static __m128i aes_decrypt_x86(__m128i input, const __m128i* key, int nr)
{
    __m128i tmp;
    tmp = _mm_xor_si128(input, _mm_load_si128(key + 0));
    tmp = _mm_aesdec_si128(tmp, _mm_load_si128(key + 1));
    tmp = _mm_aesdec_si128(tmp, _mm_load_si128(key + 2));
    tmp = _mm_aesdec_si128(tmp, _mm_load_si128(key + 3));
    tmp = _mm_aesdec_si128(tmp, _mm_load_si128(key + 4));
    tmp = _mm_aesdec_si128(tmp, _mm_load_si128(key + 5));
    tmp = _mm_aesdec_si128(tmp, _mm_load_si128(key + 6));
    tmp = _mm_aesdec_si128(tmp, _mm_load_si128(key + 7));
    tmp = _mm_aesdec_si128(tmp, _mm_load_si128(key + 8));
    tmp = _mm_aesdec_si128(tmp, _mm_load_si128(key + 9));
    if (nr > 10)
    {
        tmp = _mm_aesdec_si128(tmp, _mm_load_si128(key + 10));
        tmp = _mm_aesdec_si128(tmp, _mm_load_si128(key + 11));
        tmp = _mm_aesdec_si128(tmp, _mm_load_si128(key + 12));
        tmp = _mm_aesdec_si128(tmp, _mm_load_si128(key + 13));
    }
    return _mm_aesdeclast_si128(tmp, _mm_load_si128(key + nr));
}

void aes_ecb_encrypt_x86(const aes_context* ctx, const uint8_t* input, uint8_t* output)
{
    const __m128i* key = (__m128i*)ctx->key;
    __m128i tmp = aes_encrypt_x86(_mm_loadu_si128((const __m128i*)input), key, ctx->nr);
    _mm_storeu_si128((__m128i*)output, tmp);
}

void aes_ecb_decrypt_x86(const aes_context* ctx, const uint8_t* input, uint8_t* output)
{
    const __m128i* key = (__m128i*)ctx->key;
    __m128i tmp = aes_decrypt_x86(_mm_loadu_si128((const __m128i*)input), key, ctx->nr);
    _mm_storeu_si128((__m128i*)output, tmp);
}

static __m128i ctr_increment(__m128i counter)
{
    __m128i swap = _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    __m128i tmp = _mm_shuffle_epi8(counter, swap);
    tmp = _mm_add_epi64(tmp, _mm_set_epi32(0, 0, 0, 1));
    return _mm_shuffle_epi8(tmp, swap);
}

void aes_ctr_xor_x86(const aes_context* ctx, const uint8_t* iv, uint8_t* buffer, size_t size)
{
    const __m128i* key = (__m128i*)ctx->key;
    __m128i counter = _mm_loadu_si128((const __m128i*)iv);

    while (size >= 16)
    {
        __m128i block = aes_encrypt_x86(counter, key, ctx->nr);
        __m128i tmp = _mm_xor_si128(_mm_loadu_si128((const __m128i*)buffer), block);
        _mm_storeu_si128((__m128i*)buffer, tmp);

        counter = ctr_increment(counter);

        buffer += 16;
        size -= 16;
    }

    if (size != 0)
    {
        uint8_t full[16];
        memcpy(full, buffer, size);
        memset(full + size, 0, 16 - size);

        __m128i block = aes_encrypt_x86(counter, key, ctx->nr);
        __m128i tmp = _mm_xor_si128(_mm_loadu_si128((const __m128i*)full), block);
        _mm_storeu_si128((__m128i*)full, tmp);

        memcpy(buffer, full, size);
    }
}

void aes_cmac_process_x86(const aes_context* ctx, uint8_t* block, const uint8_t* buffer, uint32_t size)
{
    const __m128i* key = (__m128i*)ctx->key;
    __m128i* data = (__m128i*)buffer;

    __m128i tmp = _mm_loadu_si128((__m128i*)block);
    for (uint32_t i = 0; i < size; i += 16)
    {
        __m128i input = _mm_loadu_si128(data++);
        tmp = _mm_xor_si128(tmp, input);
        tmp = aes_encrypt_x86(tmp, key, ctx->nr);
    }
    _mm_storeu_si128((__m128i*)block, tmp);
}

void aes_psp_decrypt_x86(const aes_context* ctx, const uint8_t* prev, const uint8_t* block, uint8_t* buffer, uint32_t size)
{
    const __m128i* key = (__m128i*)ctx->key;
    __m128i one = _mm_setr_epi32(0, 0, 0, 1);

    __m128i x = _mm_load_si128((__m128i*)prev);
    __m128i y = _mm_load_si128((__m128i*)block);

    __m128i* data = (__m128i*)buffer;

    for (uint32_t i = 0; i < size; i += 16)
    {
        y = _mm_add_epi32(y, one);

        __m128i out = aes_decrypt_x86(y, key, ctx->nr);

        out = _mm_xor_si128(out, _mm_loadu_si128(data));
        out = _mm_xor_si128(out, x);
        _mm_storeu_si128(data++, out);
        x = y;
    }
}
