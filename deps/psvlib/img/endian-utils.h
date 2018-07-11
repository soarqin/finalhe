#ifndef ENDIAN_UTILS_H
#define ENDIAN_UTILS_H

#ifdef __APPLE__

#include <machine/endian.h>
#include <libkern/OSByteOrder.h>

#define htobe16(x) OSSwapHostToBigInt16(x)
#define htole16(x) OSSwapHostToLittleInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)
 
#define htobe32(x) OSSwapHostToBigInt32(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)
 
#define htobe64(x) OSSwapHostToBigInt64(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)

#define __BYTE_ORDER    BYTE_ORDER
#define __BIG_ENDIAN    BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#define __PDP_ENDIAN    PDP_ENDIAN

#else

// Taken from FreeBSD
#define	bswap16(x) (uint16_t) \
	((x >> 8) | (x << 8))

#define	bswap32(x) (uint32_t) \
	((x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24))

#define	bswap64(x) (uint64_t) \
	((x >> 56) | ((x >> 40) & 0xff00) | ((x >> 24) & 0xff0000) | \
	((x >> 8) & 0xff000000) | ((x << 8) & ((uint64_t)0xff << 32)) | \
	((x << 24) & ((uint64_t)0xff << 40)) | \
	((x << 40) & ((uint64_t)0xff << 48)) | ((x << 56)))

/*
 * Host to big endian, host to little endian, big endian to host, and little
 * endian to host byte order functions as detailed in byteorder(9).
 */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define	htobe16(x)	bswap16((uint16_t)(x))
#define	htobe32(x)	bswap32((uint32_t)(x))
#define	htobe64(x)	bswap64((uint64_t)(x))
#define	htole16(x)	((uint16_t)(x))
#define	htole32(x)	((uint32_t)(x))
#define	htole64(x)	((uint64_t)(x))

#define	be16toh(x)	bswap16((uint16_t)(x))
#define	be32toh(x)	bswap32((uint32_t)(x))
#define	be64toh(x)	bswap64((uint64_t)(x))
#define	le16toh(x)	((uint16_t)(x))
#define	le32toh(x)	((uint32_t)(x))
#define	le64toh(x)	((uint64_t)(x))
#else /* _BYTE_ORDER != _LITTLE_ENDIAN */
#define	htobe16(x)	((uint16_t)(x))
#define	htobe32(x)	((uint32_t)(x))
#define	htobe64(x)	((uint64_t)(x))
#define	htole16(x)	bswap16((uint16_t)(x))
#define	htole32(x)	bswap32((uint32_t)(x))
#define	htole64(x)	bswap64((uint64_t)(x))

#define	be16toh(x)	((uint16_t)(x))
#define	be32toh(x)	((uint32_t)(x))
#define	be64toh(x)	((uint64_t)(x))
#define	le16toh(x)	bswap16((uint16_t)(x))
#define	le32toh(x)	bswap32((uint32_t)(x))
#define	le64toh(x)	bswap64((uint64_t)(x))
#endif /* _BYTE_ORDER == _LITTLE_ENDIAN */

#endif

#endif
