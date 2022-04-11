#ifndef _INIT_INITRAMFS_H_
#define _INIT_INITRAMFS_H_

#include <types.h>
#include <util.h>
#include <printf.h>

#define CPIO_HEADER_MAGIC "070701"
#define CPIO_FOOTER_MAGIC "TRAILER!!!"

#define MINORBITS 20
#define MINORMASK ((1U << MINORBITS) - 1)

#define MAJOR(dev) ((uint32_t) ((dev) >> MINORBITS))
#define MINOR(dev) ((uint32_t) ((dev) & MINORMASK))
#define MKDEV(ma, mi) (((ma) << MINORBITS) | (mi))

extern char *cpio_start, *cpio_end;

typedef struct _Cpio_header
{
    uint64_t c_ino;      /* I-number of file */
    uint8_t c_mode;      /* File mode */
    uid_t c_uid;         /* Owner user ID */
    gid_t c_gid;         /* Owner uroup ID */
    uint64_t c_nlink;    /* Number of links to file */
    time64_t c_mtime;    /* Modification time */
    uint64_t c_filesize; /* Length of file */
    uint64_t c_devmajor; /* Major device number */
    uint64_t c_devminor; /* Minor device number */
    uint32_t c_rdev;     /* Device major/minor for special file */
    uint64_t c_namesize; /* Length of filename */
} Cpio_header;

char *cpio_find_file(const char *fname);
void cpio_ls();

#endif /* _INIT_INITRAMFS_H_ */