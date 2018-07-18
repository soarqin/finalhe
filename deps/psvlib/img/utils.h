/* Copyright (C) 2017 Yifan Lu
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#ifndef UTILS_H__
#define UTILS_H__

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __GNUC__
#define ATTR_PACKED __attribute__((packed))
#else
#define ATTR_PACKED
#endif

#ifdef _MSC_VER
 /* ssize_t is also not available (copy/paste from MinGW) */
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
#undef ssize_t
#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef int ssize_t;
#endif /* _WIN64 */
#endif /* _SSIZE_T_DEFINED */
#endif /* _MSC_VER */

#ifndef O_BINARY
#define O_BINARY 0
#endif

ssize_t read_block(int fd, void *buf, size_t nbyte);
ssize_t write_block(int fd, const void *buf, size_t nbyte);
ssize_t copy_block(int fd_out, int fd_in, size_t nbyte);
int parse_key(const char *ascii, uint8_t key[0x20]);

#endif
