#include <init/initramfs.h>
#include <types.h>
#include <lib/printf.h>

char *cpio_start, *cpio_end;

uint32_t new_encode_dev(dev_t dev)
{
	unsigned major = MAJOR(dev);
	unsigned minor = MINOR(dev);
	return (minor & 0xff) | (major << 8) | ((minor & ~0xff) << 12);
}

/* name of file in cpio is 4-byte alignment */
#define N_ALIGN(len) ((((len) + 1) & ~3) + 2)
int cpio_find_file(char *archive, const char *target)
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
            break;

        fcontent = hdr + N_ALIGN(ramfs_header.c_namesize);        
        if (!strcmp(fname, target))
        {
            printf("%s", fcontent);
            return 0;
        }
        next_hdr = fcontent + ramfs_header.c_filesize;
    }
    
    return -1;
}

void cpio_ls(char *archive)
{
    if (!memcmp(archive, CPIO_HEADER_MAGIC, 6))
        return;

    Cpio_header ramfs_header;
    char *fname, *fcontent;
    uint64_t parsed[12];
	char buf[9];
	int i;

    char *hdr;
    char *next_hdr = archive;
    char name_buf[0x10], data_buf[0x100];
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
        
        int fname_sz = ramfs_header.c_namesize >= 0xf ? 0xf : ramfs_header.c_namesize;
        int fcnt_sz = ramfs_header.c_filesize >= 0xff ? 0xff : ramfs_header.c_filesize;

        fname = hdr + 8;
        if (!strcmp(fname, CPIO_FOOTER_MAGIC))
            break;

        memcpy(name_buf, fname, fname_sz);
        name_buf[ fname_sz ] = '\0';
        
        fcontent = fname + N_ALIGN(ramfs_header.c_namesize);        
        memcpy(data_buf, fcontent, fcnt_sz);
        data_buf[ fcnt_sz ] = '\0';

        printf("[ %s ] : %s\r\n", name_buf, data_buf);
        next_hdr = fcontent + ramfs_header.c_filesize;
        next_hdr = (char *) (((uint64_t) next_hdr + 3) & ~3);
    }
}