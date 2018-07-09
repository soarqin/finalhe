/* This file is generated automatically by configure */

#ifndef __BYTEORDER_H
#define __BYTEORDER_H

/* Use these as generic byteswapping macros on this little endian system */
#define swap16(x)		ntohs(x)
#define swap32(x)		ntohl(x)
/* No optimized 64 bit byte swapping macro is available */
#define swap64(x) ((uint64_t)(((uint64_t)(x) << 56) & 0xff00000000000000ULL | \
			      ((uint64_t)(x) << 40) & 0x00ff000000000000ULL | \
			      ((uint64_t)(x) << 24) & 0x0000ff0000000000ULL | \
			      ((uint64_t)(x) << 8)  & 0x000000ff00000000ULL | \
			      ((x) >> 8)  & 0x00000000ff000000ULL | \
			      ((x) >> 24) & 0x0000000000ff0000ULL | \
			      ((x) >> 40) & 0x000000000000ff00ULL | \
			      ((x) >> 56) & 0x00000000000000ffULL))

/* The byte swapping macros have the form: */
/*   EENN[a]toh or htoEENN[a] where EE is be (big endian) or */
/* le (little-endian), NN is 16 or 32 (number of bits) and a, */
/* if present, indicates that the endian side is a pointer to an */
/* array of uint8_t bytes instead of an integer of the specified length. */
/* h refers to the host's ordering method. */

/* So, to convert a 32-bit integer stored in a buffer in little-endian */
/* format into a uint32_t usable on this machine, you could use: */
/*   uint32_t value = le32atoh(&buf[3]); */
/* To put that value back into the buffer, you could use: */
/*   htole32a(&buf[3], value); */

/* Define aliases for the standard byte swapping macros */
/* Arguments to these macros must be properly aligned on natural word */
/* boundaries in order to work properly on all architectures */
#ifndef htobe16
# define htobe16(x) htons(x)
#endif
#ifndef htobe32
# define htobe32(x) htonl(x)
#endif
#ifndef be16toh
# define be16toh(x) ntohs(x)
#endif
#ifndef be32toh
# define be32toh(x) ntohl(x)
#endif

#define HTOBE16(x) (x) = htobe16(x)
#define HTOBE32(x) (x) = htobe32(x)
#define BE32TOH(x) (x) = be32toh(x)
#define BE16TOH(x) (x) = be16toh(x)

/* On little endian machines, these macros are null */
#ifndef htole16
# define htole16(x)      (x)
#endif
#ifndef htole32
# define htole32(x)      (x)
#endif
#ifndef htole64
# define htole64(x)      (x)
#endif
#ifndef le16toh
# define le16toh(x)      (x)
#endif
#ifndef le32toh
# define le32toh(x)      (x)
#endif
#ifndef le64toh
# define le64toh(x)      (x)
#endif

#define HTOLE16(x)      (void) (x)
#define HTOLE32(x)      (void) (x)
#define HTOLE64(x)      (void) (x)
#define LE16TOH(x)      (void) (x)
#define LE32TOH(x)      (void) (x)
#define LE64TOH(x)      (void) (x)

/* These don't have standard aliases */
#ifndef htobe64
# define htobe64(x)      swap64(x)
#endif
#ifndef be64toh
# define be64toh(x)      swap64(x)
#endif

#define HTOBE64(x)      (x) = htobe64(x)
#define BE64TOH(x)      (x) = be64toh(x)

/* Define the C99 standard length-specific integer types */
#include <stdint.h>

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

#endif /*__BYTEORDER_H*/
