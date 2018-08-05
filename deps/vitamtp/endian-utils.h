#ifndef ENDIAN_UTILS_H
#define ENDIAN_UTILS_H

#include <stdint.h>

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
	((x >> 24) | ((x >> 8) & 0xff00U) | ((x << 8) & 0xff0000U) | (x << 24))

#define	bswap64(x) (uint64_t) \
	((x >> 56) | ((x >> 40) & 0xff00ULL) | ((x >> 24) & 0xff0000ULL) | \
	((x >> 8) & 0xff000000ULL) | ((x << 8) & (0xffULL << 32)) | \
	((x << 24) & (0xffULL << 40)) | \
	((x << 40) & (0xffULL << 48)) | ((x << 56)))

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

#define HTOBE16(x) (x) = htobe16(x)
#define HTOBE32(x) (x) = htobe32(x)
#define HTOBE64(x) (x) = htobe64(x)
#define BE16TOH(x) (x) = be16toh(x)
#define BE32TOH(x) (x) = be32toh(x)
#define BE64TOH(x) (x) = be64toh(x)

#define HTOLE16(x) (x) = htole16(x)
#define HTOLE32(x) (x) = htole32(x)
#define HTOLE64(x) (x) = htole64(x)
#define LE16TOH(x) (x) = le16toh(x)
#define LE32TOH(x) (x) = le32toh(x)
#define LE64TOH(x) (x) = le64toh(x)

/* Here are some macros to create integers from a byte array */
/* These are used to get and put integers from/into a uint8_t array */
/* with a specific endianness.  This is the most portable way to generate */
/* and read messages to a network or serial device.  Each member of a */
/* packet structure must be handled separately. */

/* The i386 and compatibles can handle unaligned memory access, */
/* so use the optimized macros above to do this job */
#ifndef be16atoh
# define be16atoh(x)     be16toh(*(uint16_t*)(x))
#endif
#ifndef be32atoh
# define be32atoh(x)     be32toh(*(uint32_t*)(x))
#endif
#ifndef be64atoh
# define be64atoh(x)     be64toh(*(uint64_t*)(x))
#endif
#ifndef le16atoh
# define le16atoh(x)     le16toh(*(uint16_t*)(x))
#endif
#ifndef le32atoh
# define le32atoh(x)     le32toh(*(uint32_t*)(x))
#endif
#ifndef le64atoh
# define le64atoh(x)     le64toh(*(uint64_t*)(x))
#endif

#ifndef htob16a
# define htobe16a(a,x)   *(uint16_t*)(a) = htobe16(x)
#endif
#ifndef htobe32a
# define htobe32a(a,x)   *(uint32_t*)(a) = htobe32(x)
#endif
#ifndef htobe64a
# define htobe64a(a,x)   *(uint64_t*)(a) = htobe64(x)
#endif
#ifndef htole16a
# define htole16a(a,x)   *(uint16_t*)(a) = htole16(x)
#endif
#ifndef htole32a
# define htole32a(a,x)   *(uint32_t*)(a) = htole32(x)
#endif
#ifndef htole64a
# define htole64a(a,x)   *(uint64_t*)(a) = htole64(x)
#endif

#endif
