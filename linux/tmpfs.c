#include <tmpfs.h>
#include <fs.h>
#include <util.h>

int tmpfs_setup_mount(const struct filesystem *fs, struct mount *mount);

const struct filesystem tmpfs = {
    .name = "tmpfs",
    .setup_mount = tmpfs_setup_mount,
};

int tmpfs_setup_mount(const struct filesystem* fs, struct mount* mount)
{
    return 0;
}