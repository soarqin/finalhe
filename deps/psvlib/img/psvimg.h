/* Copyright (C) 2017 Yifan Lu
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#ifndef PSVIMG_H__
#define PSVIMG_H__

#include <stdint.h>

#ifdef __GNUC__
#define ATTR_PACKED __attribute__((packed))
#else
#define ATTR_PACKED
#endif

#define PSVMD_CONTENT_MAGIC (0xFEE1900D)
#define PSVMD_BACKUP_MAGIC (0xFEE1900E)
#define PSVIMG_ENDOFHEADER "EndOfHeader\n"
#define PSVIMG_ENDOFTAILER "EndOfTailer\n"
#define PSVIMG_HEADER_FILLER ('x')
#define PSVIMG_TAILER_FILLER ('z')
#define PSVIMG_PADDING_FILLER ('+')
#define PSVIMG_ENTRY_ALIGN (0x400)
#define PSVIMG_BLOCK_SIZE (0x8000)

/** Access modes for st_mode in SceIoStat (confirm?). */
enum {
  /** Format bits mask */
  SCE_S_IFMT    = 0xF000,
  /** Symbolic link */
  SCE_S_IFLNK   = 0x4000,
  /** Directory */
  SCE_S_IFDIR   = 0x1000,
  /** Regular file */
  SCE_S_IFREG   = 0x2000,

  /** Set UID */
  SCE_S_ISUID   = 0x0800,
  /** Set GID */
  SCE_S_ISGID   = 0x0400,
  /** Sticky */
  SCE_S_ISVTX   = 0x0200,

  /** Others access rights mask */
  SCE_S_IRWXO   = 0x01C0,
  /** Others read permission */
  SCE_S_IROTH   = 0x0100,
  /** Others write permission */
  SCE_S_IWOTH   = 0x0080,
  /** Others execute permission */
  SCE_S_IXOTH   = 0x0040,

  /** Group access rights mask */
  SCE_S_IRWXG   = 0x0038,
  /** Group read permission */
  SCE_S_IRGRP   = 0x0020,
  /** Group write permission */
  SCE_S_IWGRP   = 0x0010,
  /** Group execute permission */
  SCE_S_IXGRP   = 0x0008,

  /** User access rights mask */
  SCE_S_IRWXU   = 0x0007,
  /** User read permission */
  SCE_S_IRUSR   = 0x0004,
  /** User write permission */
  SCE_S_IWUSR   = 0x0002,
  /** User execute permission */
  SCE_S_IXUSR   = 0x0001,
};

// File mode checking macros
#define SCE_S_ISLNK(m)  (((m) & SCE_S_IFMT) == SCE_S_IFLNK)
#define SCE_S_ISREG(m)  (((m) & SCE_S_IFMT) == SCE_S_IFREG)
#define SCE_S_ISDIR(m)  (((m) & SCE_S_IFMT) == SCE_S_IFDIR)

/** File modes, used for the st_attr parameter in SceIoStat (confirm?). */
enum {
  /** Format mask */
  SCE_SO_IFMT             = 0x0038,               // Format mask
  /** Symlink */
  SCE_SO_IFLNK            = 0x0008,               // Symbolic link
  /** Directory */
  SCE_SO_IFDIR            = 0x0010,               // Directory
  /** Regular file */
  SCE_SO_IFREG            = 0x0020,               // Regular file

  /** Hidden read permission */
  SCE_SO_IROTH            = 0x0004,               // read
  /** Hidden write permission */
  SCE_SO_IWOTH            = 0x0002,               // write
  /** Hidden execute permission */
  SCE_SO_IXOTH            = 0x0001,               // execute
};

// File mode checking macros
#define SCE_SO_ISLNK(m) (((m) & SCE_SO_IFMT) == SCE_SO_IFLNK)
#define SCE_SO_ISREG(m) (((m) & SCE_SO_IFMT) == SCE_SO_IFREG)
#define SCE_SO_ISDIR(m) (((m) & SCE_SO_IFMT) == SCE_SO_IFDIR)

#pragma pack(push, 1)
typedef struct SceDateTime {
    uint16_t year;
    uint16_t month;
    uint16_t day;
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
    uint32_t microsecond;
} ATTR_PACKED SceDateTime;

/** Structure to hold the status information about a file */
typedef struct SceIoStat {
  uint32_t sst_mode;
  unsigned int  sst_attr;
  /** Size of the file in bytes. */
  uint64_t  sst_size;
  /** Creation time. */
  SceDateTime sst_ctime;
  /** Access time. */
  SceDateTime sst_atime;
  /** Modification time. */
  SceDateTime sst_mtime;
  /** Device-specific data. */
  uint32_t  sst_private[6];
} ATTR_PACKED SceIoStat;

#define PSVMD_FW_1_00 (0x01000000)

typedef struct PsvMd {
  uint32_t magic;
  uint32_t type;
  uint64_t fw_version;
  uint8_t  psid[16];
  char     name[64];
  uint64_t psvimg_size;
  uint64_t version; // only support 2
  uint64_t total_size;
  uint8_t  iv[16];
  uint64_t ux0_info;
  uint64_t ur0_info;
  uint64_t unused_98;
  uint64_t unused_A0;
  uint32_t add_data;
} ATTR_PACKED PsvMd_t;

/** This file (and backup) can only be restored with the same PSID */
#define PSVIMG_HEADER_FLAG_CONSOLE_UNIQUE (0x1)

typedef struct PsvImgHeader {
  uint64_t  systime;
  uint64_t  flags;
  SceIoStat stat;
  char      path_parent[256];
  uint32_t  unk_16C; // set to 1
  char      path_rel[256];
  char      unused[904];
  char      end[12];
} ATTR_PACKED PsvImgHeader_t;

/** The file/directory will be _removed_ (not restored). */
#define PSVIMG_TAILER_FLAG_REMOVE (0x1)

typedef struct PsvImgTailer {
  uint64_t  flags;
  char      unused[1004];
  char      end[12];
} ATTR_PACKED PsvImgTailer_t;
#pragma pack(pop)

#endif
