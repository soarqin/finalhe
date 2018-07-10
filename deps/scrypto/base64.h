#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    BASE64_OK = 0,
    ERR_BASE64_BUFFER_TOO_SMALL = -1,
    ERR_BASE64_INVALID_CHARACTER = -2,
};

int base64_encode( uint8_t *dst, size_t dlen, size_t *olen,
                   const uint8_t *src, size_t slen );
int base64_decode( uint8_t *dst, size_t dlen, size_t *olen,
                   const uint8_t *src, size_t slen );

#ifdef __cplusplus
}
#endif
