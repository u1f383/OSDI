#include <initramfs.h>
#include <types.h>
#include <printf.h>
#include <sched.h>
#include <fs.h>

uint64_t cpio_start = NULL,
         cpio_end = NULL;

const struct filesystem initramfs = {
    .config = FS_RDONLY,
    .name = "initramfs",
    .setup_mount = initramfs_setup_mount,
    .fops = &file_ops,
};

/* Name of file in cpio is 4-byte alignment */
#define N_ALIGN(len) ((((len) + 1) & ~3) + 2)
int initramfs_setup_mount(const struct filesystem* fs, struct mount* mount)
{
    if (cpio_start == NULL)
        return -1;

    if (memcmp((void *)cpio_start, CPIO_HEADER_MAGIC, 6))
        return -1;

    struct vnode *prev, *vnode;
    CpioHeader ramfs_header;
    uint64_t parsed[12];
    char *fname;
	char buf[9];
    char *hdr;
    char name_buf[256];
    char component_name[16];
    int i;

    char *next_hdr = (char *)cpio_start;
    current->workdir = mount->root;
    while (1)
    {
        hdr = next_hdr;
        buf[8] = '\0';
        for (i = 0, hdr += 6; i < 12; i++, hdr += 8) {
            memcpy(buf, hdr, 8);
            parsed[i] = strtolu(buf, 16);
        }

        ramfs_header.c_mode = parsed[1];
        ramfs_header.c_filesize = parsed[6];
        ramfs_header.c_namesize = parsed[11];

        fname = hdr + 8;

        if (!strcmp(fname, "."))
            goto _next_file;

        if (!strcmp(fname, CPIO_FOOTER_MAGIC))
            break;

        memcpy(name_buf, fname, ramfs_header.c_namesize);
        name_buf[ramfs_header.c_namesize] = '\0';
        ramfs_header.content = fname + N_ALIGN(ramfs_header.c_namesize);

        vnode = NULL;
        if (__vfs_lookup(name_buf, component_name, &prev, &vnode) != 0)
            return -1;

        if (vnode != NULL)
            return -1;
        
        if (S_ISDIR(ramfs_header.c_mode)) {
            prev->v_ops->create(prev, &vnode, &file_ops, component_name, FILE_DIR);
        } else {
            prev->v_ops->create(prev, &vnode, &file_ops, component_name, FILE_NORM);
            vnode->size = ramfs_header.c_filesize;
            vnode->internal.mem = (char *) ramfs_header.content;
        }

    _next_file:
        next_hdr = fname + N_ALIGN(ramfs_header.c_namesize) + ramfs_header.c_filesize;
        next_hdr = (char *)(((uint64_t) next_hdr + 3) & ~3);
    }
    
    current->workdir = rootfs->root;
    return 0;
}
