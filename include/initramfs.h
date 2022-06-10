#ifndef _INITRAMFS_H_
#define _INITRAMFS_H_

#include <types.h>
#include <util.h>
#include <fs.h>

#define CPIO_HEADER_MAGIC "070701"
#define CPIO_FOOTER_MAGIC "TRAILER!!!"

#define MINORBITS 20
#define MINORMASK ((1U << MINORBITS) - 1)

#define MAJOR(dev) ((uint32_t) ((dev) >> MINORBITS))
#define MINOR(dev) ((uint32_t) ((dev) & MINORMASK))
#define MKDEV(ma, mi) (((ma) << MINORBITS) | (mi))

extern uint64_t cpio_start, cpio_end;
extern const struct filesystem initramfs;

#define S_IFMT 00170000
#define S_IFDIR 0040000
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)

typedef struct _CpioHeader
{
    uint64_t c_ino;      /* I-number of file */
    uint16_t c_mode;      /* File mode */
    uid_t c_uid;         /* Owner user ID */
    gid_t c_gid;         /* Owner uroup ID */
    uint64_t c_nlink;    /* Number of links to file */
    time64_t c_mtime;    /* Modification time */
    uint64_t c_filesize; /* Length of file */
    uint64_t c_devmajor; /* Major device number */
    uint64_t c_devminor; /* Minor device number */
    uint32_t c_rdev;     /* Device major/minor for special file */
    uint64_t c_namesize; /* Length of filename */
    const char *content;
} CpioHeader;

int initramfs_setup_mount(const struct filesystem *fs, struct mount *mount);

#endif /* _INITRAMFS_H_ */