#include "sc_crc32.h"

#ifdef __arm64__
#include "sse2neon.h"
#else
#include <wmmintrin.h> // PCLMUL
#include <tmmintrin.h> // SSSE3
#include <smmintrin.h> // SSS4
#endif

// Whitepaper: https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/fast-crc-computation-generic-polynomials-pclmulqdq-paper.pdf
// ZLIB licensed code from https://github.com/jtkukunas/zlib/blob/master/crc_folding.c

static const uint32_t _ALIGNED(16) shift_table[] = {
    0x84838281, 0x88878685, 0x8c8b8a89, 0x008f8e8d,
    0x85848382, 0x89888786, 0x8d8c8b8a, 0x01008f8e,
    0x86858483, 0x8a898887, 0x8e8d8c8b, 0x0201008f,
    0x87868584, 0x8b8a8988, 0x8f8e8d8c, 0x03020100,
    0x88878685, 0x8c8b8a89, 0x008f8e8d, 0x04030201,
    0x89888786, 0x8d8c8b8a, 0x01008f8e, 0x05040302,
    0x8a898887, 0x8e8d8c8b, 0x0201008f, 0x06050403,
    0x8b8a8988, 0x8f8e8d8c, 0x03020100, 0x07060504,
    0x8c8b8a89, 0x008f8e8d, 0x04030201, 0x08070605,
    0x8d8c8b8a, 0x01008f8e, 0x05040302, 0x09080706,
    0x8e8d8c8b, 0x0201008f, 0x06050403, 0x0a090807,
    0x8f8e8d8c, 0x03020100, 0x07060504, 0x0b0a0908,
    0x008f8e8d, 0x04030201, 0x08070605, 0x0c0b0a09,
    0x01008f8e, 0x05040302, 0x09080706, 0x0d0c0b0a,
    0x0201008f, 0x06050403, 0x0a090807, 0x0e0d0c0b,
};

#define FOLD1(xmm0, xmm1, xmm2, xmm3) do         \
{                                                \
    const __m128i fold4 = _mm_set_epi32(         \
        0x00000001, 0x54442bd4,                  \
        0x00000001, 0xc6e41596);                 \
                                                 \
    __m128i r0, r1, r2, r3, a, b;                \
                                                 \
    r0 = xmm1;                                   \
    r1 = xmm2;                                   \
    r2 = xmm3;                                   \
                                                 \
    a = _mm_clmulepi64_si128(xmm0, fold4, 0x01); \
    b = _mm_clmulepi64_si128(xmm0, fold4, 0x10); \
    r3 = _mm_xor_si128(a, b);                    \
                                                 \
    xmm0 = r0;                                   \
    xmm1 = r1;                                   \
    xmm2 = r2;                                   \
    xmm3 = r3;                                   \
} while (0)

#define FOLD2(xmm0, xmm1, xmm2, xmm3) do         \
{                                                \
    const __m128i fold4 = _mm_set_epi32(         \
        0x00000001, 0x54442bd4,                  \
        0x00000001, 0xc6e41596);                 \
                                                 \
    __m128i r0, r1, r2, r3, a, b;                \
                                                 \
    r0 = xmm2;                                   \
    r1 = xmm3;                                   \
                                                 \
    a = _mm_clmulepi64_si128(xmm0, fold4, 0x01); \
    b = _mm_clmulepi64_si128(xmm0, fold4, 0x10); \
    r2 = _mm_xor_si128(a, b);                    \
                                                 \
    a = _mm_clmulepi64_si128(xmm1, fold4, 0x01); \
    b = _mm_clmulepi64_si128(xmm1, fold4, 0x10); \
    r3 = _mm_xor_si128(a, b);                    \
                                                 \
    xmm0 = r0;                                   \
    xmm1 = r1;                                   \
    xmm2 = r2;                                   \
    xmm3 = r3;                                   \
} while (0)

#define FOLD3(xmm0, xmm1, xmm2, xmm3) do         \
{                                                \
    const __m128i fold4 = _mm_set_epi32(         \
        0x00000001, 0x54442bd4,                  \
        0x00000001, 0xc6e41596);                 \
                                                 \
    __m128i r0, r1, r2, r3, a, b;                \
                                                 \
    r0 = xmm3;                                   \
                                                 \
    a = _mm_clmulepi64_si128(xmm0, fold4, 0x01); \
    b = _mm_clmulepi64_si128(xmm0, fold4, 0x10); \
    r1 = _mm_xor_si128(a, b);                    \
                                                 \
    a = _mm_clmulepi64_si128(xmm1, fold4, 0x01); \
    b = _mm_clmulepi64_si128(xmm1, fold4, 0x10); \
    r2 = _mm_xor_si128(a, b);                    \
                                                 \
    a = _mm_clmulepi64_si128(xmm2, fold4, 0x01); \
    b = _mm_clmulepi64_si128(xmm2, fold4, 0x10); \
    r3 = _mm_xor_si128(a, b);                    \
                                                 \
    xmm0 = r0;                                   \
    xmm1 = r1;                                   \
    xmm2 = r2;                                   \
    xmm3 = r3;                                   \
} while (0)

#define FOLD4(xmm0, xmm1, xmm2, xmm3) do         \
{                                                \
    const __m128i fold4 = _mm_set_epi32(         \
        0x00000001, 0x54442bd4,                  \
        0x00000001, 0xc6e41596);                 \
                                                 \
    __m128i a, b;                                \
                                                 \
    a = _mm_clmulepi64_si128(xmm0, fold4, 0x01); \
    b = _mm_clmulepi64_si128(xmm0, fold4, 0x10); \
    xmm0 = _mm_xor_si128(a, b);                  \
                                                 \
    a = _mm_clmulepi64_si128(xmm1, fold4, 0x01); \
    b = _mm_clmulepi64_si128(xmm1, fold4, 0x10); \
    xmm1 = _mm_xor_si128(a, b);                  \
                                                 \
    a = _mm_clmulepi64_si128(xmm2, fold4, 0x01); \
    b = _mm_clmulepi64_si128(xmm2, fold4, 0x10); \
    xmm2 = _mm_xor_si128(a, b);                  \
                                                 \
    a = _mm_clmulepi64_si128(xmm3, fold4, 0x01); \
    b = _mm_clmulepi64_si128(xmm3, fold4, 0x10); \
    xmm3 = _mm_xor_si128(a, b);                  \
} while (0)

#define PARTIAL(len, xmm0, xmm1, xmm2, xmm3, xmm4) do                 \
{                                                                     \
    const __m128i fold4 = _mm_set_epi32(                              \
        0x00000001, 0x54442bd4,                                       \
        0x00000001, 0xc6e41596);                                      \
    const __m128i mask = _mm_set1_epi32(0x80808080);                  \
                                                                      \
    __m128i shl = _mm_load_si128((__m128i *)shift_table + (len - 1)); \
    __m128i shr = _mm_xor_si128(shl, mask);                           \
                                                                      \
    __m128i a, b, r;                                                  \
    __m128i tmp = _mm_shuffle_epi8(xmm0, shl);                        \
                                                                      \
    a = _mm_shuffle_epi8(xmm0, shr);                                  \
    b = _mm_shuffle_epi8(xmm1, shl);                                  \
    xmm0 = _mm_or_si128(a, b);                                        \
                                                                      \
    a = _mm_shuffle_epi8(xmm1, shr);                                  \
    b = _mm_shuffle_epi8(xmm2, shl);                                  \
    xmm1 = _mm_or_si128(a, b);                                        \
                                                                      \
    a = _mm_shuffle_epi8(xmm2, shr);                                  \
    b = _mm_shuffle_epi8(xmm3, shl);                                  \
    xmm2 = _mm_or_si128(a, b);                                        \
                                                                      \
    a = _mm_shuffle_epi8(xmm3, shr);                                  \
    b = _mm_shuffle_epi8(xmm4, shl);                                  \
    xmm4 = b;                                                         \
    r = _mm_or_si128(a, b);                                           \
                                                                      \
    a = _mm_clmulepi64_si128(tmp, fold4, 0x10);                       \
    b = _mm_clmulepi64_si128(tmp, fold4, 0x01);                       \
                                                                      \
    r = _mm_xor_si128(r, a);                                          \
    r = _mm_xor_si128(r, b);                                          \
    xmm3 = r;                                                         \
} while(0)

void crc32_init_x86(crc32_ctx* ctx)
{
    __m128i init = _mm_cvtsi32_si128(0x9db42487);
    __m128i zero = _mm_setzero_si128();

    _mm_store_si128((__m128i*)ctx->crc + 0, init);
    _mm_store_si128((__m128i*)ctx->crc + 1, zero);
    _mm_store_si128((__m128i*)ctx->crc + 2, zero);
    _mm_store_si128((__m128i*)ctx->crc + 3, zero);
}

void crc32_update_x86(crc32_ctx* ctx, const void* buffer, size_t size)
{
    const uint8_t* buffer8 = buffer;

    __m128i xmm0 = _mm_load_si128((__m128i*)ctx->crc + 0);
    __m128i xmm1 = _mm_load_si128((__m128i*)ctx->crc + 1);
    __m128i xmm2 = _mm_load_si128((__m128i*)ctx->crc + 2);
    __m128i xmm3 = _mm_load_si128((__m128i*)ctx->crc + 3);
    __m128i xmm4 = _mm_load_si128((__m128i*)ctx->crc + 4);

    if (size < 16)
    {
        if (size == 0)
        {
            return;
        }
        xmm4 = _mm_loadu_si128((__m128i *)buffer8);
        goto partial;
    }

    uint32_t prefix = (0 - (uintptr_t)buffer8) & 0xF;
    if (prefix != 0)
    {
        xmm4 = _mm_loadu_si128((__m128i *)buffer8);
        buffer8 += prefix;
        size -= prefix;

        PARTIAL(prefix, xmm0, xmm1, xmm2, xmm3, xmm4);
    }

    while (size >= 64)
    {
        __m128i t0 = _mm_load_si128((__m128i *)buffer8 + 0);
        __m128i t1 = _mm_load_si128((__m128i *)buffer8 + 1);
        __m128i t2 = _mm_load_si128((__m128i *)buffer8 + 2);
        __m128i t3 = _mm_load_si128((__m128i *)buffer8 + 3);

        FOLD4(xmm0, xmm1, xmm2, xmm3);

        xmm0 = _mm_xor_si128(xmm0, t0);
        xmm1 = _mm_xor_si128(xmm1, t1);
        xmm2 = _mm_xor_si128(xmm2, t2);
        xmm3 = _mm_xor_si128(xmm3, t3);

        buffer8 += 64;
        size -= 64;
    }

    if (size >= 48)
    {
        __m128i t0 = _mm_load_si128((__m128i *)buffer8 + 0);
        __m128i t1 = _mm_load_si128((__m128i *)buffer8 + 1);
        __m128i t2 = _mm_load_si128((__m128i *)buffer8 + 2);

        FOLD3(xmm0, xmm1, xmm2, xmm3);

        xmm1 = _mm_xor_si128(xmm1, t0);
        xmm2 = _mm_xor_si128(xmm2, t1);
        xmm3 = _mm_xor_si128(xmm3, t2);

        buffer8 += 48;
        size -= 48;
    }
    else if (size >= 32)
    {
        __m128i t0 = _mm_load_si128((__m128i *)buffer8 + 0);
        __m128i t1 = _mm_load_si128((__m128i *)buffer8 + 1);

        FOLD2(xmm0, xmm1, xmm2, xmm3);

        xmm2 = _mm_xor_si128(xmm2, t0);
        xmm3 = _mm_xor_si128(xmm3, t1);

        buffer8 += 32;
        size -= 32;
    }
    else if (size >= 16)
    {
        __m128i t0 = _mm_load_si128((__m128i *)buffer8 + 0);

        FOLD1(xmm0, xmm1, xmm2, xmm3);

        xmm3 = _mm_xor_si128(xmm3, t0);

        buffer8 += 16;
        size -= 16;
    }

    if (size == 0)
    {
        goto done;
    }

    xmm4 = _mm_load_si128((__m128i *)buffer8);

partial:
    PARTIAL(size, xmm0, xmm1, xmm2, xmm3, xmm4);

done:
    _mm_store_si128((__m128i*)ctx->crc + 0, xmm0);
    _mm_store_si128((__m128i*)ctx->crc + 1, xmm1);
    _mm_store_si128((__m128i*)ctx->crc + 2, xmm2);
    _mm_store_si128((__m128i*)ctx->crc + 3, xmm3);
    _mm_store_si128((__m128i*)ctx->crc + 4, xmm4);
}

uint32_t crc32_done_x86(crc32_ctx* ctx)
{
    const __m128i mask1 = _mm_setr_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000);
    const __m128i mask2 = _mm_setr_epi32(0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);

    __m128i xmm0 = _mm_load_si128((__m128i*)ctx->crc + 0);
    __m128i xmm1 = _mm_load_si128((__m128i*)ctx->crc + 1);
    __m128i xmm2 = _mm_load_si128((__m128i*)ctx->crc + 2);
    __m128i xmm3 = _mm_load_si128((__m128i*)ctx->crc + 3);

    __m128i fold;
    __m128i a, b, t;

    fold = _mm_setr_epi32(0xccaa009e, 0x00000000, 0x751997d0, 0x00000001);

    a = _mm_clmulepi64_si128(xmm0, fold, 0x10);
    b = _mm_clmulepi64_si128(xmm0, fold, 0x01);
    t = _mm_xor_si128(xmm1, a);
    t = _mm_xor_si128(t, b);

    a = _mm_clmulepi64_si128(t, fold, 0x10);
    b = _mm_clmulepi64_si128(t, fold, 0x01);
    t = _mm_xor_si128(xmm2, a);
    t = _mm_xor_si128(t, b);

    a = _mm_clmulepi64_si128(t, fold, 0x10);
    b = _mm_clmulepi64_si128(t, fold, 0x01);
    t = _mm_xor_si128(xmm3, a);
    t = _mm_xor_si128(t, b);

    fold = _mm_setr_epi32(0xccaa009e, 0x00000000, 0x63cd6124, 0x00000001);

    a = _mm_clmulepi64_si128(t, fold, 0);
    b = _mm_srli_si128(t, 8);
    a = _mm_xor_si128(a, b);

    b = _mm_slli_si128(a, 4);
    b = _mm_clmulepi64_si128(b, fold, 0x10);
    t = _mm_xor_si128(a, b);
    t = _mm_and_si128(t, mask2);

    fold = _mm_setr_epi32(0xf7011640, 0x00000001, 0xdb710640, 0x00000001);

    a = _mm_clmulepi64_si128(t, fold, 0);
    a = _mm_xor_si128(a, t);
    a = _mm_and_si128(a, mask1);

    b = _mm_clmulepi64_si128(a, fold, 0x10);
    b = _mm_xor_si128(b, a);
    b = _mm_xor_si128(b, t);

    uint32_t crc = _mm_extract_epi32(b, 2);
    return ~crc;
}
