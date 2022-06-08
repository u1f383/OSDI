#include <fs.h>
#include <tmpfs.h>
#include <mm.h>
#include <types.h>
#include <util.h>
#include <sched.h>

struct mount *rootfs = NULL;

const struct vnode_operations vnode_ops = {
    .create = vfs_create,
    .lookup = vfs_lookup,
    .mkdir = vfs_mkdir,
};

const struct file_operations file_ops = {
    .open = vfs_open,
    .write = vfs_write,
    .read = vfs_read,
    .close = vfs_close,
    .lseek64 = vfs_lseek64,
};

static struct file *new_file(struct vnode *vnode, int flags)
{
    struct file *file = kmalloc(sizeof(struct file));
    file->f_ops = &file_ops;
    file->f_pos = 0;
    file->flags = flags;
    file->vnode = vnode;
    return file;
}

static struct vnode *new_vnode(struct vnode *parent,
            const char *component_name, uint8_t type)
{
    struct vnode *vn = kmalloc(sizeof(struct vnode));
    memset(vn->component_name, 0, FILE_COMPONENT_NAME_LEN);
    strcpy(vn->component_name, component_name);
    LIST_INIT(vn->list);
    vn->f_ops = &file_ops;
    vn->v_ops = &vnode_ops;
    vn->parent = parent;
    vn->type = type;
    vn->mount = NULL;
    vn->internal.mem = NULL;
    return vn;
}

int register_filesystem(const struct filesystem *fs)
{
    if (rootfs != NULL)
        return -1;

    rootfs = kmalloc(sizeof(struct mount));
    rootfs->fs = fs;

    struct vnode *vnode = kmalloc(sizeof(struct vnode));
    vnode->mount = rootfs;
    vnode->v_ops = &vnode_ops;
    vnode->f_ops = &file_ops;
    vnode->internal.mem = NULL;
    memset(vnode->component_name, 0, FILE_COMPONENT_NAME_LEN);
    strcpy(vnode->component_name, "/");
    LIST_INIT(vnode->list);
    rootfs->root = vnode;
    
    fs->setup_mount(fs, rootfs);
    return 0;
}

static int get_root_vnode(const char *pathname, struct vnode **vnode)
{
    if (!memcmp(pathname, "/", 1)) {
        *vnode = rootfs->root;
        return 1;
    } else if (!memcmp(pathname, "../", 3)) {
        *vnode = current->workdir->parent;
        return 3;
    } else if (!memcmp(pathname, "./", 2)) {
        *vnode = current->workdir;
        return 2;
    }
    
    *vnode = current->workdir;
    return 0;
}

static int __vfs_lookup(const char *pathname, char *component_name,
                        struct vnode **prev, struct vnode **target)
{
    int i, j;
    int pathlen = strlen(pathname);

    i = get_root_vnode(pathname, target);
    for (; i < pathlen; i++) {
        *prev = *target;
        *target = NULL;

        while (pathname[i] == '/') i++;
        for (j = 0; pathname[i] != '/' && i < pathlen; j++, i++)
            component_name[j] = pathname[i];
        component_name[j] = '\0';

        (*prev)->v_ops->lookup(*prev, target, component_name);
        if (i != pathlen && *target == NULL)
            return -1;
    }
    return 0;
}

int vfs_open(const char *pathname, int flags, struct file **target)
{
    char component_name[FILE_COMPONENT_NAME_LEN];
    struct vnode *dir_node, *vnode;

    if (__vfs_lookup(pathname, component_name, &dir_node, &vnode) != 0)
        return -1;

    if (vnode == NULL) {
        if (!(flags & 00000100))
            return -1;
        
        dir_node->v_ops->create(dir_node, &vnode, component_name, FILE_NORM);
    }

    struct file *file = new_file(vnode, flags);
    *target = file;

    return 0;
}

int vfs_close(struct file *file)
{
    kfree(file);
    return 0;
}

int vfs_write(struct file *file, const void *buf, uint64_t len)
{
    if (file->vnode->type == FILE_DIR)
        return -1;

    const char *ptr = buf;
    int i;
    for (i = 0; i < len && file->f_pos < FILE_MAX_SIZE; i++, file->f_pos++)
        *(file->vnode->internal.mem + file->f_pos) = *ptr++;

    return i;
}

int vfs_read(struct file *file, void *buf, uint64_t len)
{
    if (file->vnode->type == FILE_DIR)
        return -1;

    char *ptr = buf;
    int i;
    for (i = 0; i < len && file->f_pos < FILE_MAX_SIZE; i++, file->f_pos++)
        *ptr++ = *(file->vnode->internal.mem + file->f_pos);

    return i;
}

long vfs_lseek64(struct file *file, long offset, int whence)
{
    file->f_pos = whence + offset;
    return 0;
}

// ======================================

int vfs_mount(const char *target, const char *filesystem)
{
    const struct filesystem *fs = NULL;

    if (!strcmp(filesystem, "tmpfs")) {
        fs = &tmpfs;
    } else {
        return -1;
    }

    char component_name[FILE_COMPONENT_NAME_LEN];
    struct vnode *dir_node, *vnode;

    if (__vfs_lookup(target, component_name, &dir_node, &vnode) != 0)
        return -1;

    if (vnode == NULL)
        return -1;

    struct mount *new_mount_point = kmalloc(sizeof(struct mount));
    new_mount_point->fs = fs;
    new_mount_point->root = vnode;
    vnode->mount = new_mount_point;

    return 0;
}

int vfs_lookup(struct vnode *dir_node, struct vnode **target,
                const char *component_name)
{
    struct vnode *first_file = dir_node->internal.next_layer;
    struct vnode *fiter = first_file;

    do {
        if (!strcmp(fiter->component_name, component_name)) {
            *target = fiter;
            return 0;
        }

        fiter = container_of(first_file->list.next, struct vnode, list);
    } while (fiter != first_file);

    return -1;
}

int vfs_create(struct vnode *dir_node, struct vnode **target,
                const char *component_name, uint8_t type)
{
    struct vnode *first = dir_node->internal.next_layer;
    struct vnode *vn = new_vnode(dir_node, component_name, type);

    list_add_tail(&vn->list, &first->list);
    *target = vn;
    return 0;
}

int vfs_mkdir(const char *pathname)
{
    char component_name[FILE_COMPONENT_NAME_LEN];
    struct vnode *dir_node, *vnode;

    if (__vfs_lookup(pathname, component_name, &dir_node, &vnode) != 0)
        return -1;

    if (vnode != NULL)
        return -1;
    
    dir_node->v_ops->create(dir_node, &vnode, component_name, FILE_DIR);
    return 0;
}

// ======================================

int svc_open(const char *pathname, int flags)
{
    int fdt_idx;
    for (fdt_idx = 0; fdt_idx < FDT_SIZE; fdt_idx++)
        if (current->fdt->files[fdt_idx] == NULL)
            break;

    if (fdt_idx == FDT_SIZE)
        return -1;

    struct file *file = NULL;
    rootfs->root->f_ops->open(pathname, flags, &file);

    if (file == NULL) {
        current->fdt->files[fdt_idx] = file;
        return fdt_idx;
    }
    return -1;
}

int svc_close(int fd)
{
    if ((uint32_t)fd >= FDT_SIZE || current->fdt->files[fd] == NULL)
        return -1;
    
    current->fdt->files[fd]->f_ops->close(current->fdt->files[fd]);
    current->fdt->files[fd] = NULL;

    return 0;
}

long svc_write(int fd, const void *buf, unsigned long count)
{
    if ((uint32_t)fd >= FDT_SIZE || current->fdt->files[fd] == NULL)
        return -1;

    return current->fdt->files[fd]->f_ops->write(current->fdt->files[fd], buf, count);
}

long svc_read(int fd, void *buf, unsigned long count)
{
    if ((uint32_t)fd >= FDT_SIZE || current->fdt->files[fd] == NULL)
        return -1;

    return current->fdt->files[fd]->f_ops->read(current->fdt->files[fd], buf, count);
}


int svc_mkdir(const char *pathname, unsigned mode)
{
    return rootfs->root->v_ops->mkdir(pathname);
}

int svc_mount(const char *src, const char *target, const char *filesystem, unsigned long flags, const void *data)
{
    return vfs_mount(target, filesystem);
}

int svc_chdir(const char *path)
{
    struct vnode *dir_node, *vnode;
    char component_name[FILE_COMPONENT_NAME_LEN];

    if (__vfs_lookup(path, component_name, &dir_node, &vnode) != 0)
        return -1;

    if (vnode == NULL)
        return -1;

    current->workdir = vnode;
    return 0;
}

long svc_lseek64(int fd, long offset, int whence)
{
    if ((uint32_t)fd >= FDT_SIZE || current->fdt->files[fd] == NULL)
        return -1;

    return current->fdt->files[fd]->f_ops->lseek64(current->fdt->files[fd], offset, whence);
}

int svc_ioctl(int fd, unsigned long request, ...)
{

}