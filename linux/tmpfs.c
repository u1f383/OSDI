#include <tmpfs.h>
#include <fs.h>
#include <util.h>

const struct filesystem tmpfs = {
    .config = 0,
    .name = "tmpfs",
    .setup_mount = tmpfs_setup_mount,
    .fops = &file_ops,
};

int tmpfs_setup_mount(const struct filesystem* fs, struct mount* mount)
{
    return 0;
}