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
static uint32_t new_encode_dev(dev_t dev)
{
	unsigned major = MAJOR(dev);
	unsigned minor = MINOR(dev);
	return (minor & 0xff) | (major << 8) | ((minor & ~0xff) << 12);
}

typedef struct _Cpio_header
{
    uint64_t c_ino; /* I-number of file */
    uint8_t c_mode; /* File mode */
    uid_t c_uid; /* Owner user ID */
    gid_t c_gid; /* Owner uroup ID */
    uint64_t c_nlink; /* Number of links to file */
    time64_t c_mtime; /* Modification time */
    uint64_t c_filesize; /* Length of file */
    uint64_t c_devmajor; /* Major device number */
    uint64_t c_devminor; /* Minor device number */
    uint32_t c_rdev; /* Device major/minor for special file */
    uint64_t c_namesize; /* Length of filename */
} Cpio_header;

/* name of file in cpio is 4-byte alignment */
#define N_ALIGN(len) ((((len) + 1) & ~3) + 2)
int cpio_parse(char *archive)
{
    if (!memcmp(archive, CPIO_HEADER_MAGIC, 6))
        return -1;

    Cpio_header ramfs_header;
    char *fname, *fcontent;
    uint64_t parsed[12];
	char buf[9];
	int i;

    char *hdr;
    char *next_hdr = archive;
    while (1)
    {
        hdr = next_hdr;

        buf[8] = '\0';
        for (i = 0, hdr += 6; i < 12; i++, hdr += 8) {
            memcpy(buf, hdr, 8);
            parsed[i] = strtolu(buf, 16);
        }
        ramfs_header.c_ino = parsed[0];
        ramfs_header.c_mode = parsed[1];
        ramfs_header.c_uid = parsed[2];
        ramfs_header.c_gid = parsed[3];
        ramfs_header.c_nlink = parsed[4];
        ramfs_header.c_mtime = parsed[5];
        ramfs_header.c_filesize = parsed[6];
        ramfs_header.c_devmajor = parsed[7];
        ramfs_header.c_devminor = parsed[8];
        ramfs_header.c_rdev = new_encode_dev(MKDEV(parsed[9], parsed[10]));
        ramfs_header.c_namesize = parsed[11];

        fname = hdr + sizeof(Cpio_header);
        if (!strcmp(fname, CPIO_FOOTER_MAGIC))
            return 1;
        
        fcontent = hdr + N_ALIGN(ramfs_header.c_namesize);
        next_hdr = fcontent + ramfs_header.c_filesize;
    }
    
    return 0;
}