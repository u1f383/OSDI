#include <initramfs.h>
#include <types.h>
#include <printf.h>

uint64_t cpio_start = NULL,
         cpio_end = NULL;

static uint32_t new_encode_dev(dev_t dev)
{
	unsigned major = MAJOR(dev);
	unsigned minor = MINOR(dev);
	return (minor & 0xff) | (major << 8) | ((minor & ~0xff) << 12);
}

/* Name of file in cpio is 4-byte alignment */
#define N_ALIGN(len) ((((len) + 1) & ~3) + 2)
int32_t cpio_find_file(const char *target, CpioHeader *cpio_obj)
{
    if (!cpio_start)
        return -1;

    if (memcmp((void *)cpio_start, CPIO_HEADER_MAGIC, 6))
        return -1;

    CpioHeader ramfs_header;
    char *fname;
    uint64_t parsed[12];
	char buf[9];
	int i;

    char *hdr;
    char *next_hdr = (char *)cpio_start;
    char name_buf[0x20];

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
        
        int fname_sz = ramfs_header.c_namesize >= 0x1f ? 0x1f : ramfs_header.c_namesize;

        fname = hdr + 8;
        if (!strcmp(fname, CPIO_FOOTER_MAGIC))
            break;

        memcpy(name_buf, fname, fname_sz);
        name_buf[fname_sz] = '\0';
        ramfs_header.content = fname + N_ALIGN(ramfs_header.c_namesize);

        if (!strcmp(name_buf, target)) {
            memcpy(cpio_obj, &ramfs_header, sizeof(CpioHeader));
            return 0;
        }

        next_hdr = (char *)ramfs_header.content + ramfs_header.c_filesize;
        next_hdr = (char *)(((uint64_t) next_hdr + 3) & ~3);
    }

    return -1;
}
