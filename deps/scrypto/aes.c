#include "aes.h"

#include <stddef.h>
#include <assert.h>
#include <string.h>

static inline uint32_t get32le(const uint8_t* bytes)
{
    return (bytes[0]) | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}

static inline uint32_t get32be(const uint8_t* bytes)
{
    return (bytes[3]) | (bytes[2] << 8) | (bytes[1] << 16) | (bytes[0] << 24);
}

static inline void set32le(uint8_t* bytes, uint32_t x)
{
    bytes[0] = (uint8_t)x;
    bytes[1] = (uint8_t)(x >> 8);
    bytes[2] = (uint8_t)(x >> 16);
    bytes[3] = (uint8_t)(x >> 24);
}

static inline void set32be(uint8_t* bytes, uint32_t x)
{
    bytes[0] = (uint8_t)(x >> 24);
    bytes[1] = (uint8_t)(x >> 16);
    bytes[2] = (uint8_t)(x >> 8);
    bytes[3] = (uint8_t)x;
}

#if defined(_MSC_VER)
#define PLATFORM_SUPPORTS_AESNI 1

#include <intrin.h>
static void get_cpuid(uint32_t level, uint32_t* arr)
{
    __cpuidex((int*)arr, level, 0);
}

#elif defined(__x86_64__) || defined(__i386__)
#define PLATFORM_SUPPORTS_AESNI 1

#include <cpuid.h>
static void get_cpuid(uint32_t level, uint32_t* arr)
{
    __cpuid_count(level, 0, arr[0], arr[1], arr[2], arr[3]);
}

#else
#define PLATFORM_SUPPORTS_AESNI 0
#endif

#if PLATFORM_SUPPORTS_AESNI
static int aes_supported_x86()
{
    return 0;
    static int init = 0;
    static int supported;
    if (!init)
    {
        init = 1;

        uint32_t a[4];
        get_cpuid(0, a);

        if (a[0] >= 1)
        {
            get_cpuid(1, a);
            supported = ((a[2] & (1 << 9)) && (a[2] & (1 << 25)));
        }
    }
    return supported;
}

void aes_init_x86(aes_context* context, const uint8_t* key);
void aes_init_dec_x86(aes_context* context, const uint8_t* key);
void aes_ecb_encrypt_x86(const aes_context* context, const uint8_t* input, uint8_t* output);
void aes_ecb_decrypt_x86(const aes_context* context, const uint8_t* input, uint8_t* output);
void aes_ctr_xor_x86(const aes_context* context, const uint8_t* iv, uint8_t* buffer, size_t size);
void aes_cmac_process_x86(const aes_context* ctx, uint8_t* block, const uint8_t *buffer, uint32_t size);
void aes_psp_decrypt_x86(const aes_context* ctx, const uint8_t* prev, const uint8_t* block, uint8_t* buffer, uint32_t size);
#endif

static const uint8_t rcon[] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36,
};

static const uint8_t Te[] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

static uint8_t Td[] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d,
};

static const uint32_t TE[] = {
    0xc66363a5, 0xf87c7c84, 0xee777799, 0xf67b7b8d, 0xfff2f20d, 0xd66b6bbd, 0xde6f6fb1, 0x91c5c554,
    0x60303050, 0x02010103, 0xce6767a9, 0x562b2b7d, 0xe7fefe19, 0xb5d7d762, 0x4dababe6, 0xec76769a,
    0x8fcaca45, 0x1f82829d, 0x89c9c940, 0xfa7d7d87, 0xeffafa15, 0xb25959eb, 0x8e4747c9, 0xfbf0f00b,
    0x41adadec, 0xb3d4d467, 0x5fa2a2fd, 0x45afafea, 0x239c9cbf, 0x53a4a4f7, 0xe4727296, 0x9bc0c05b,
    0x75b7b7c2, 0xe1fdfd1c, 0x3d9393ae, 0x4c26266a, 0x6c36365a, 0x7e3f3f41, 0xf5f7f702, 0x83cccc4f,
    0x6834345c, 0x51a5a5f4, 0xd1e5e534, 0xf9f1f108, 0xe2717193, 0xabd8d873, 0x62313153, 0x2a15153f,
    0x0804040c, 0x95c7c752, 0x46232365, 0x9dc3c35e, 0x30181828, 0x379696a1, 0x0a05050f, 0x2f9a9ab5,
    0x0e070709, 0x24121236, 0x1b80809b, 0xdfe2e23d, 0xcdebeb26, 0x4e272769, 0x7fb2b2cd, 0xea75759f,
    0x1209091b, 0x1d83839e, 0x582c2c74, 0x341a1a2e, 0x361b1b2d, 0xdc6e6eb2, 0xb45a5aee, 0x5ba0a0fb,
    0xa45252f6, 0x763b3b4d, 0xb7d6d661, 0x7db3b3ce, 0x5229297b, 0xdde3e33e, 0x5e2f2f71, 0x13848497,
    0xa65353f5, 0xb9d1d168, 0x00000000, 0xc1eded2c, 0x40202060, 0xe3fcfc1f, 0x79b1b1c8, 0xb65b5bed,
    0xd46a6abe, 0x8dcbcb46, 0x67bebed9, 0x7239394b, 0x944a4ade, 0x984c4cd4, 0xb05858e8, 0x85cfcf4a,
    0xbbd0d06b, 0xc5efef2a, 0x4faaaae5, 0xedfbfb16, 0x864343c5, 0x9a4d4dd7, 0x66333355, 0x11858594,
    0x8a4545cf, 0xe9f9f910, 0x04020206, 0xfe7f7f81, 0xa05050f0, 0x783c3c44, 0x259f9fba, 0x4ba8a8e3,
    0xa25151f3, 0x5da3a3fe, 0x804040c0, 0x058f8f8a, 0x3f9292ad, 0x219d9dbc, 0x70383848, 0xf1f5f504,
    0x63bcbcdf, 0x77b6b6c1, 0xafdada75, 0x42212163, 0x20101030, 0xe5ffff1a, 0xfdf3f30e, 0xbfd2d26d,
    0x81cdcd4c, 0x180c0c14, 0x26131335, 0xc3ecec2f, 0xbe5f5fe1, 0x359797a2, 0x884444cc, 0x2e171739,
    0x93c4c457, 0x55a7a7f2, 0xfc7e7e82, 0x7a3d3d47, 0xc86464ac, 0xba5d5de7, 0x3219192b, 0xe6737395,
    0xc06060a0, 0x19818198, 0x9e4f4fd1, 0xa3dcdc7f, 0x44222266, 0x542a2a7e, 0x3b9090ab, 0x0b888883,
    0x8c4646ca, 0xc7eeee29, 0x6bb8b8d3, 0x2814143c, 0xa7dede79, 0xbc5e5ee2, 0x160b0b1d, 0xaddbdb76,
    0xdbe0e03b, 0x64323256, 0x743a3a4e, 0x140a0a1e, 0x924949db, 0x0c06060a, 0x4824246c, 0xb85c5ce4,
    0x9fc2c25d, 0xbdd3d36e, 0x43acacef, 0xc46262a6, 0x399191a8, 0x319595a4, 0xd3e4e437, 0xf279798b,
    0xd5e7e732, 0x8bc8c843, 0x6e373759, 0xda6d6db7, 0x018d8d8c, 0xb1d5d564, 0x9c4e4ed2, 0x49a9a9e0,
    0xd86c6cb4, 0xac5656fa, 0xf3f4f407, 0xcfeaea25, 0xca6565af, 0xf47a7a8e, 0x47aeaee9, 0x10080818,
    0x6fbabad5, 0xf0787888, 0x4a25256f, 0x5c2e2e72, 0x381c1c24, 0x57a6a6f1, 0x73b4b4c7, 0x97c6c651,
    0xcbe8e823, 0xa1dddd7c, 0xe874749c, 0x3e1f1f21, 0x964b4bdd, 0x61bdbddc, 0x0d8b8b86, 0x0f8a8a85,
    0xe0707090, 0x7c3e3e42, 0x71b5b5c4, 0xcc6666aa, 0x904848d8, 0x06030305, 0xf7f6f601, 0x1c0e0e12,
    0xc26161a3, 0x6a35355f, 0xae5757f9, 0x69b9b9d0, 0x17868691, 0x99c1c158, 0x3a1d1d27, 0x279e9eb9,
    0xd9e1e138, 0xebf8f813, 0x2b9898b3, 0x22111133, 0xd26969bb, 0xa9d9d970, 0x078e8e89, 0x339494a7,
    0x2d9b9bb6, 0x3c1e1e22, 0x15878792, 0xc9e9e920, 0x87cece49, 0xaa5555ff, 0x50282878, 0xa5dfdf7a,
    0x038c8c8f, 0x59a1a1f8, 0x09898980, 0x1a0d0d17, 0x65bfbfda, 0xd7e6e631, 0x844242c6, 0xd06868b8,
    0x824141c3, 0x299999b0, 0x5a2d2d77, 0x1e0f0f11, 0x7bb0b0cb, 0xa85454fc, 0x6dbbbbd6, 0x2c16163a,
};

static const uint32_t TD[] = {
    0x51f4a750, 0x7e416553, 0x1a17a4c3, 0x3a275e96, 0x3bab6bcb, 0x1f9d45f1, 0xacfa58ab, 0x4be30393,
    0x2030fa55, 0xad766df6, 0x88cc7691, 0xf5024c25, 0x4fe5d7fc, 0xc52acbd7, 0x26354480, 0xb562a38f,
    0xdeb15a49, 0x25ba1b67, 0x45ea0e98, 0x5dfec0e1, 0xc32f7502, 0x814cf012, 0x8d4697a3, 0x6bd3f9c6,
    0x038f5fe7, 0x15929c95, 0xbf6d7aeb, 0x955259da, 0xd4be832d, 0x587421d3, 0x49e06929, 0x8ec9c844,
    0x75c2896a, 0xf48e7978, 0x99583e6b, 0x27b971dd, 0xbee14fb6, 0xf088ad17, 0xc920ac66, 0x7dce3ab4,
    0x63df4a18, 0xe51a3182, 0x97513360, 0x62537f45, 0xb16477e0, 0xbb6bae84, 0xfe81a01c, 0xf9082b94,
    0x70486858, 0x8f45fd19, 0x94de6c87, 0x527bf8b7, 0xab73d323, 0x724b02e2, 0xe31f8f57, 0x6655ab2a,
    0xb2eb2807, 0x2fb5c203, 0x86c57b9a, 0xd33708a5, 0x302887f2, 0x23bfa5b2, 0x02036aba, 0xed16825c,
    0x8acf1c2b, 0xa779b492, 0xf307f2f0, 0x4e69e2a1, 0x65daf4cd, 0x0605bed5, 0xd134621f, 0xc4a6fe8a,
    0x342e539d, 0xa2f355a0, 0x058ae132, 0xa4f6eb75, 0x0b83ec39, 0x4060efaa, 0x5e719f06, 0xbd6e1051,
    0x3e218af9, 0x96dd063d, 0xdd3e05ae, 0x4de6bd46, 0x91548db5, 0x71c45d05, 0x0406d46f, 0x605015ff,
    0x1998fb24, 0xd6bde997, 0x894043cc, 0x67d99e77, 0xb0e842bd, 0x07898b88, 0xe7195b38, 0x79c8eedb,
    0xa17c0a47, 0x7c420fe9, 0xf8841ec9, 0x00000000, 0x09808683, 0x322bed48, 0x1e1170ac, 0x6c5a724e,
    0xfd0efffb, 0x0f853856, 0x3daed51e, 0x362d3927, 0x0a0fd964, 0x685ca621, 0x9b5b54d1, 0x24362e3a,
    0x0c0a67b1, 0x9357e70f, 0xb4ee96d2, 0x1b9b919e, 0x80c0c54f, 0x61dc20a2, 0x5a774b69, 0x1c121a16,
    0xe293ba0a, 0xc0a02ae5, 0x3c22e043, 0x121b171d, 0x0e090d0b, 0xf28bc7ad, 0x2db6a8b9, 0x141ea9c8,
    0x57f11985, 0xaf75074c, 0xee99ddbb, 0xa37f60fd, 0xf701269f, 0x5c72f5bc, 0x44663bc5, 0x5bfb7e34,
    0x8b432976, 0xcb23c6dc, 0xb6edfc68, 0xb8e4f163, 0xd731dcca, 0x42638510, 0x13972240, 0x84c61120,
    0x854a247d, 0xd2bb3df8, 0xaef93211, 0xc729a16d, 0x1d9e2f4b, 0xdcb230f3, 0x0d8652ec, 0x77c1e3d0,
    0x2bb3166c, 0xa970b999, 0x119448fa, 0x47e96422, 0xa8fc8cc4, 0xa0f03f1a, 0x567d2cd8, 0x223390ef,
    0x87494ec7, 0xd938d1c1, 0x8ccaa2fe, 0x98d40b36, 0xa6f581cf, 0xa57ade28, 0xdab78e26, 0x3fadbfa4,
    0x2c3a9de4, 0x5078920d, 0x6a5fcc9b, 0x547e4662, 0xf68d13c2, 0x90d8b8e8, 0x2e39f75e, 0x82c3aff5,
    0x9f5d80be, 0x69d0937c, 0x6fd52da9, 0xcf2512b3, 0xc8ac993b, 0x10187da7, 0xe89c636e, 0xdb3bbb7b,
    0xcd267809, 0x6e5918f4, 0xec9ab701, 0x834f9aa8, 0xe6956e65, 0xaaffe67e, 0x21bccf08, 0xef15e8e6,
    0xbae79bd9, 0x4a6f36ce, 0xea9f09d4, 0x29b07cd6, 0x31a4b2af, 0x2a3f2331, 0xc6a59430, 0x35a266c0,
    0x744ebc37, 0xfc82caa6, 0xe090d0b0, 0x33a7d815, 0xf104984a, 0x41ecdaf7, 0x7fcd500e, 0x1791f62f,
    0x764dd68d, 0x43efb04d, 0xccaa4d54, 0xe49604df, 0x9ed1b5e3, 0x4c6a881b, 0xc12c1fb8, 0x4665517f,
    0x9d5eea04, 0x018c355d, 0xfa877473, 0xfb0b412e, 0xb3671d5a, 0x92dbd252, 0xe9105633, 0x6dd64713,
    0x9ad7618c, 0x37a10c7a, 0x59f8148e, 0xeb133c89, 0xcea927ee, 0xb761c935, 0xe11ce5ed, 0x7a47b13c,
    0x9cd2df59, 0x55f2733f, 0x1814ce79, 0x73c737bf, 0x53f7cdea, 0x5ffdaa5b, 0xdf3d6f14, 0x7844db86,
    0xcaaff381, 0xb968c43e, 0x3824342c, 0xc2a3405f, 0x161dc372, 0xbce2250c, 0x283c498b, 0xff0d9541,
    0x39a80171, 0x080cb3de, 0xd8b4e49c, 0x6456c190, 0x7bcb8461, 0xd532b670, 0x486c5c74, 0xd0b85742,
};

static inline uint8_t byte32(uint32_t x, int n)
{
    return (uint8_t)(x >> (8 * n));
}

static inline uint32_t ror32(uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

static inline uint32_t setup_mix(uint32_t x)
{
    return (Te[byte32(x, 2)] << 24) ^ (Te[byte32(x, 1)] << 16) ^ (Te[byte32(x, 0)] << 8) ^ Te[byte32(x, 3)];
}

static inline uint32_t setup_mix2(uint32_t x)
{
    return TD[Te[byte32(x, 3)]] ^ ror32(TD[Te[byte32(x, 2)]], 8) ^ ror32(TD[Te[byte32(x, 1)]], 16) ^ ror32(TD[Te[byte32(x, 0)]], 24);
}

static inline uint32_t setup_mix3(uint32_t x)
{
    return (Te[byte32(x, 3)] << 24) ^ (Te[byte32(x, 2)] << 16) ^ (Te[byte32(x, 1)] << 8) ^ Te[byte32(x, 0)];
}

int aes_init(aes_context* ctx, const uint8_t* key, int bits)
{
    switch(bits) {
        case 128: ctx->nr = 10; break;
        case 256: ctx->nr = 14; break;
        default: return -1;
    }
#if PLATFORM_SUPPORTS_AESNI
    if (aes_supported_x86())
    {
        aes_init_x86(ctx, key);
        return 0;
    }
#endif

    uint32_t* ekey = ctx->key;
    switch(bits) {
        case 128:
            ekey[0] = get32be(key +  0);
            ekey[1] = get32be(key +  4);
            ekey[2] = get32be(key +  8);
            ekey[3] = get32be(key + 12);

            for (size_t i=0; i<10; i++)
            {
                ekey[4] = ekey[0] ^ setup_mix(ekey[3]) ^ (rcon[i] << 24);
                ekey[5] = ekey[1] ^ ekey[4];
                ekey[6] = ekey[2] ^ ekey[5];
                ekey[7] = ekey[3] ^ ekey[6];
                ekey += 4;
            }
            break;
        case 256:
            ekey[0] = get32be(key +  0);
            ekey[1] = get32be(key +  4);
            ekey[2] = get32be(key +  8);
            ekey[3] = get32be(key + 12);
            ekey[4] = get32be(key + 16);
            ekey[5] = get32be(key + 20);
            ekey[6] = get32be(key + 24);
            ekey[7] = get32be(key + 28);

            for (size_t i=0; i<7; i++)
            {
                ekey[8]  = ekey[0] ^ setup_mix(ekey[7]) ^ (rcon[i] << 24);
                ekey[9]  = ekey[1] ^ ekey[8];
                ekey[10] = ekey[2] ^ ekey[9];
                ekey[11] = ekey[3] ^ ekey[10];
                ekey[12] = ekey[4] ^ setup_mix3(ekey[11]);
                ekey[13] = ekey[5] ^ ekey[12];
                ekey[14] = ekey[6] ^ ekey[13];
                ekey[15] = ekey[7] ^ ekey[14];
                ekey += 8;
            }
            break;
    }

    return 0;
}

int aes_init_dec(aes_context* ctx, const uint8_t* key, int bits)
{
    switch(bits) {
        case 128: ctx->nr = 10; break;
        case 256: ctx->nr = 14; break;
        default: return -1;
    }
#if PLATFORM_SUPPORTS_AESNI
    if (aes_supported_x86())
    {
        aes_init_dec_x86(ctx, key);
        return 0;
    }
#endif

    aes_context enc;
    if (aes_init(&enc, key, bits) < 0)
        return -1;

    uint32_t* ekey = enc.key + 4 * ctx->nr;
    uint32_t* dkey = ctx->key;

    *dkey++ = ekey[0];
    *dkey++ = ekey[1];
    *dkey++ = ekey[2];
    *dkey++ = ekey[3];
    ekey -= 4;

    for (size_t i = ctx->nr - 1; i > 0; i--)
    {
        *dkey++ = setup_mix2(ekey[0]);
        *dkey++ = setup_mix2(ekey[1]);
        *dkey++ = setup_mix2(ekey[2]);
        *dkey++ = setup_mix2(ekey[3]);
        ekey -= 4;
    }

    *dkey++ = ekey[0];
    *dkey++ = ekey[1];
    *dkey++ = ekey[2];
    *dkey++ = ekey[3];
    return 0;
}

static void aes_encrypt(const aes_context* ctx, const uint8_t* input, uint8_t* output)
{
    uint32_t t0, t1, t2, t3;
    const uint32_t* key = ctx->key;

    uint32_t s0 = get32be(input + 0) ^ *key++;
    uint32_t s1 = get32be(input + 4) ^ *key++;
    uint32_t s2 = get32be(input + 8) ^ *key++;
    uint32_t s3 = get32be(input + 12) ^ *key++;

    for (size_t i = (ctx->nr>>1)-1; i > 0; i--)
    {
        t0 = TE[byte32(s0, 3)] ^ ror32(TE[byte32(s1, 2)], 8) ^ ror32(TE[byte32(s2, 1)], 16) ^ ror32(TE[byte32(s3, 0)], 24) ^ *key++;
        t1 = TE[byte32(s1, 3)] ^ ror32(TE[byte32(s2, 2)], 8) ^ ror32(TE[byte32(s3, 1)], 16) ^ ror32(TE[byte32(s0, 0)], 24) ^ *key++;
        t2 = TE[byte32(s2, 3)] ^ ror32(TE[byte32(s3, 2)], 8) ^ ror32(TE[byte32(s0, 1)], 16) ^ ror32(TE[byte32(s1, 0)], 24) ^ *key++;
        t3 = TE[byte32(s3, 3)] ^ ror32(TE[byte32(s0, 2)], 8) ^ ror32(TE[byte32(s1, 1)], 16) ^ ror32(TE[byte32(s2, 0)], 24) ^ *key++;

        s0 = TE[byte32(t0, 3)] ^ ror32(TE[byte32(t1, 2)], 8) ^ ror32(TE[byte32(t2, 1)], 16) ^ ror32(TE[byte32(t3, 0)], 24) ^ *key++;
        s1 = TE[byte32(t1, 3)] ^ ror32(TE[byte32(t2, 2)], 8) ^ ror32(TE[byte32(t3, 1)], 16) ^ ror32(TE[byte32(t0, 0)], 24) ^ *key++;
        s2 = TE[byte32(t2, 3)] ^ ror32(TE[byte32(t3, 2)], 8) ^ ror32(TE[byte32(t0, 1)], 16) ^ ror32(TE[byte32(t1, 0)], 24) ^ *key++;
        s3 = TE[byte32(t3, 3)] ^ ror32(TE[byte32(t0, 2)], 8) ^ ror32(TE[byte32(t1, 1)], 16) ^ ror32(TE[byte32(t2, 0)], 24) ^ *key++;
    }

    t0 = TE[byte32(s0, 3)] ^ ror32(TE[byte32(s1, 2)], 8) ^ ror32(TE[byte32(s2, 1)], 16) ^ ror32(TE[byte32(s3, 0)], 24) ^ *key++;
    t1 = TE[byte32(s1, 3)] ^ ror32(TE[byte32(s2, 2)], 8) ^ ror32(TE[byte32(s3, 1)], 16) ^ ror32(TE[byte32(s0, 0)], 24) ^ *key++;
    t2 = TE[byte32(s2, 3)] ^ ror32(TE[byte32(s3, 2)], 8) ^ ror32(TE[byte32(s0, 1)], 16) ^ ror32(TE[byte32(s1, 0)], 24) ^ *key++;
    t3 = TE[byte32(s3, 3)] ^ ror32(TE[byte32(s0, 2)], 8) ^ ror32(TE[byte32(s1, 1)], 16) ^ ror32(TE[byte32(s2, 0)], 24) ^ *key++;

    s0 = (Te[byte32(t0, 3)] << 24) ^ (Te[byte32(t1, 2)] << 16) ^ (Te[byte32(t2, 1)] << 8) ^ Te[byte32(t3, 0)] ^ *key++;
    s1 = (Te[byte32(t1, 3)] << 24) ^ (Te[byte32(t2, 2)] << 16) ^ (Te[byte32(t3, 1)] << 8) ^ Te[byte32(t0, 0)] ^ *key++;
    s2 = (Te[byte32(t2, 3)] << 24) ^ (Te[byte32(t3, 2)] << 16) ^ (Te[byte32(t0, 1)] << 8) ^ Te[byte32(t1, 0)] ^ *key++;
    s3 = (Te[byte32(t3, 3)] << 24) ^ (Te[byte32(t0, 2)] << 16) ^ (Te[byte32(t1, 1)] << 8) ^ Te[byte32(t2, 0)] ^ *key++;

    set32be(output + 0, s0);
    set32be(output + 4, s1);
    set32be(output + 8, s2);
    set32be(output + 12, s3);
}

static void aes_decrypt(const aes_context* ctx, const uint8_t* input, uint8_t* output)
{
    const uint32_t* key = ctx->key;

    uint32_t s0 = get32be(input + 0) ^ *key++;
    uint32_t s1 = get32be(input + 4) ^ *key++;
    uint32_t s2 = get32be(input + 8) ^ *key++;
    uint32_t s3 = get32be(input + 12) ^ *key++;

    uint32_t t0 = TD[byte32(s0, 3)] ^ ror32(TD[byte32(s3, 2)], 8) ^ ror32(TD[byte32(s2, 1)], 16) ^ ror32(TD[byte32(s1, 0)], 24) ^ *key++;
    uint32_t t1 = TD[byte32(s1, 3)] ^ ror32(TD[byte32(s0, 2)], 8) ^ ror32(TD[byte32(s3, 1)], 16) ^ ror32(TD[byte32(s2, 0)], 24) ^ *key++;
    uint32_t t2 = TD[byte32(s2, 3)] ^ ror32(TD[byte32(s1, 2)], 8) ^ ror32(TD[byte32(s0, 1)], 16) ^ ror32(TD[byte32(s3, 0)], 24) ^ *key++;
    uint32_t t3 = TD[byte32(s3, 3)] ^ ror32(TD[byte32(s2, 2)], 8) ^ ror32(TD[byte32(s1, 1)], 16) ^ ror32(TD[byte32(s0, 0)], 24) ^ *key++;

    for (size_t i = (ctx->nr>>1)-1; i > 0; i--)
    {
        s0 = TD[byte32(t0, 3)] ^ ror32(TD[byte32(t3, 2)], 8) ^ ror32(TD[byte32(t2, 1)], 16) ^ ror32(TD[byte32(t1, 0)], 24) ^ *key++;
        s1 = TD[byte32(t1, 3)] ^ ror32(TD[byte32(t0, 2)], 8) ^ ror32(TD[byte32(t3, 1)], 16) ^ ror32(TD[byte32(t2, 0)], 24) ^ *key++;
        s2 = TD[byte32(t2, 3)] ^ ror32(TD[byte32(t1, 2)], 8) ^ ror32(TD[byte32(t0, 1)], 16) ^ ror32(TD[byte32(t3, 0)], 24) ^ *key++;
        s3 = TD[byte32(t3, 3)] ^ ror32(TD[byte32(t2, 2)], 8) ^ ror32(TD[byte32(t1, 1)], 16) ^ ror32(TD[byte32(t0, 0)], 24) ^ *key++;

        t0 = TD[byte32(s0, 3)] ^ ror32(TD[byte32(s3, 2)], 8) ^ ror32(TD[byte32(s2, 1)], 16) ^ ror32(TD[byte32(s1, 0)], 24) ^ *key++;
        t1 = TD[byte32(s1, 3)] ^ ror32(TD[byte32(s0, 2)], 8) ^ ror32(TD[byte32(s3, 1)], 16) ^ ror32(TD[byte32(s2, 0)], 24) ^ *key++;
        t2 = TD[byte32(s2, 3)] ^ ror32(TD[byte32(s1, 2)], 8) ^ ror32(TD[byte32(s0, 1)], 16) ^ ror32(TD[byte32(s3, 0)], 24) ^ *key++;
        t3 = TD[byte32(s3, 3)] ^ ror32(TD[byte32(s2, 2)], 8) ^ ror32(TD[byte32(s1, 1)], 16) ^ ror32(TD[byte32(s0, 0)], 24) ^ *key++;
    }

    s0 = (Td[byte32(t0, 3)] << 24) ^ (Td[byte32(t3, 2)] << 16) ^ (Td[byte32(t2, 1)] << 8) ^ Td[byte32(t1, 0)] ^ *key++;
    s1 = (Td[byte32(t1, 3)] << 24) ^ (Td[byte32(t0, 2)] << 16) ^ (Td[byte32(t3, 1)] << 8) ^ Td[byte32(t2, 0)] ^ *key++;
    s2 = (Td[byte32(t2, 3)] << 24) ^ (Td[byte32(t1, 2)] << 16) ^ (Td[byte32(t0, 1)] << 8) ^ Td[byte32(t3, 0)] ^ *key++;
    s3 = (Td[byte32(t3, 3)] << 24) ^ (Td[byte32(t2, 2)] << 16) ^ (Td[byte32(t1, 1)] << 8) ^ Td[byte32(t0, 0)] ^ *key++;

    set32be(output + 0, s0);
    set32be(output + 4, s1);
    set32be(output + 8, s2);
    set32be(output + 12, s3);
}

void aes_ecb_encrypt(const aes_context* ctx, const uint8_t* input, uint8_t* output)
{
#if PLATFORM_SUPPORTS_AESNI
    if (aes_supported_x86())
    {
        aes_ecb_encrypt_x86(ctx, input, output);
        return;
    }
#endif
    aes_encrypt(ctx, input, output);
}

void aes_ecb_decrypt(const aes_context* ctx, const uint8_t* input, uint8_t* output)
{
#if PLATFORM_SUPPORTS_AESNI
    if (aes_supported_x86())
    {
        aes_ecb_decrypt_x86(ctx, input, output);
        return;
    }
#endif
    aes_decrypt(ctx, input, output);
}

int aes_cbc_encrypt(const aes_context* ctx, uint8_t iv[16], const uint8_t* input, size_t length, uint8_t* output) {
    int i;

    if (length % 16)
        return -1;
    while (length > 0) {
        for (i = 0; i < 16; i++)
            output[i] = (unsigned char)(input[i] ^ iv[i]);

        aes_ecb_encrypt(ctx, output, output);
        memcpy(iv, output, 16);

        input += 16;
        output += 16;
        length -= 16;
    }
    return 0;
}

int aes_cbc_decrypt(const aes_context* ctx, uint8_t iv[16], const uint8_t* input, size_t length, uint8_t* output) {
    int i;
    unsigned char temp[16];

    if (length % 16)
        return -1;
    while (length > 0) {
        memcpy(temp, input, 16);
        aes_ecb_decrypt(ctx, input, output);

        for (i = 0; i < 16; i++)
            output[i] = (unsigned char)(output[i] ^ iv[i]);

        memcpy(iv, temp, 16);

        input += 16;
        output += 16;
        length -= 16;
    }
    return 0;
}

static inline void ctr_add(uint8_t* counter, uint64_t n)
{
    for (int i=15; i>=0; i--)
    {
        n = n + counter[i];
        counter[i] = (uint8_t)n;
        n >>= 8;
    }
}

void aes_ctr_xor(const aes_context* context, const uint8_t* iv, uint64_t block, uint8_t* buffer, size_t size)
{
    uint8_t tmp[16];
    uint8_t counter[16];
    for (uint32_t i=0; i<16; i++)
    {
        counter[i] = iv[i];
    }
    ctr_add(counter, block);

#if PLATFORM_SUPPORTS_AESNI
    if (aes_supported_x86())
    {
        aes_ctr_xor_x86(context, counter, buffer, size);
        return;
    }
#endif

    while (size >= 16)
    {
        aes_encrypt(context, counter, tmp);
        for (uint32_t i=0; i<16; i++)
        {
            *buffer++ ^= tmp[i];
        }
        for (int i=15; i>=0; i--)
            if (++counter[i] != 0)
                break;
        size -= 16;
    }

    if (size != 0)
    {
        aes_encrypt(context, counter, tmp);
        for (size_t i=0; i<size; i++)
        {
            *buffer++ ^= tmp[i];
        }
    }
}

// https://tools.ietf.org/rfc/rfc4493.txt

typedef struct {
    aes_context key;
    uint8_t last[16];
    uint8_t block[16];
    uint32_t size;
} aes_cmac_ctx;

static void aes_cmac_process(const aes_context* ctx, uint8_t* block, const uint8_t *buffer, uint32_t size)
{
    assert(size % 16 == 0);

#if PLATFORM_SUPPORTS_AESNI
    if (aes_supported_x86())
    {
        aes_cmac_process_x86(ctx, block, buffer, size);
        return;
    }
#endif
    for (uint32_t i = 0; i < size; i += 16)
    {
        for (size_t k = 0; k < 16; k++)
        {
            block[k] ^= *buffer++;
        }
        aes_ecb_encrypt(ctx, block, block);
    }
}

static void aes_cmac_init(aes_cmac_ctx* ctx, const uint8_t* key)
{
    aes_init(&ctx->key, key, 128);
    memset(ctx->last, 0, 16);
    ctx->size = 0;
}

static void aes_cmac_update(aes_cmac_ctx* ctx, const uint8_t* buffer, uint32_t size)
{
    if (ctx->size + size <= 16)
    {
        memcpy(ctx->block + ctx->size, buffer, size);
        ctx->size += size;
        return;
    }

    if (ctx->size != 0)
    {
        uint32_t avail = 16 - ctx->size;
        memcpy(ctx->block + ctx->size, buffer, avail < size ? avail : size);
        buffer += avail;
        size -= avail;

        aes_cmac_process(&ctx->key, ctx->last, ctx->block, 16);
    }

    if (size >= 16)
    {
        uint32_t full = (size - 1) & ~15;
        aes_cmac_process(&ctx->key, ctx->last, buffer, full);
        buffer += full;
        size -= full;
    }

    memcpy(ctx->block, buffer, size);
    ctx->size = size;
}

static void cmac_gfmul(uint8_t* block)
{
    uint8_t carry = 0;
    for (int i = 15; i >= 0; i--)
    {
        uint8_t x = block[i];
        block[i] = (block[i] << 1) | (carry >> 7);
        carry = x;
    }

    block[15] ^= (carry & 0x80 ? 0x87 : 0);
}

static void aes_cmac_done(aes_cmac_ctx* ctx, uint8_t* mac)
{
    uint8_t zero[16] = { 0 };
    aes_ecb_encrypt(&ctx->key, zero, mac);

    cmac_gfmul(mac);

    if (ctx->size != 16)
    {
        cmac_gfmul(mac);

        ctx->block[ctx->size] = 0x80;
        memset(ctx->block + ctx->size + 1, 0, 16 - (ctx->size + 1));
    }

    for (size_t i = 0; i < 16; i++)
    {
        mac[i] ^= ctx->block[i];
    }

    aes_cmac_process(&ctx->key, mac, ctx->last, 16);
}

void aes_cmac(const uint8_t* key, const uint8_t* buffer, uint32_t size, uint8_t* mac)
{
    aes_cmac_ctx ctx;
    aes_cmac_init(&ctx, key);
    aes_cmac_update(&ctx, buffer, size);
    aes_cmac_done(&ctx, mac);
}

void aes_psp_decrypt(const aes_context* ctx, const uint8_t* iv, uint32_t index, uint8_t* buffer, uint32_t size)
{
    assert(size % 16 == 0);

    uint8_t _ALIGNED(16) prev[16];
    uint8_t _ALIGNED(16) block[16];

    if (index == 0)
    {
        memset(prev, 0, 16);
    }
    else
    {
        memcpy(prev, iv, 12);
        set32le(prev + 12, index);
    }

    memcpy(block, iv, 16);
    set32le(block + 12, index);

#if PLATFORM_SUPPORTS_AESNI
    if (aes_supported_x86())
    {
        aes_psp_decrypt_x86(ctx, prev, block, buffer, size);
        return;
    }
#endif

    for (uint32_t i = 0; i < size; i += 16)
    {
        set32le(block + 12, get32le(block + 12) + 1);

        uint8_t out[16];
        aes_ecb_decrypt(ctx, block, out);

        for (size_t k = 0; k < 16; k++)
        {
            *buffer++ ^= prev[k] ^ out[k];
        }
        memcpy(prev, block, 16);
    }
}
