#include <fs.h>
#include <tmpfs.h>
#include <mm.h>
#include <types.h>
#include <util.h>
#include <sched.h>
#include <gpio.h>
#include <fat32.h>
#include <printf.h>
#include <stdarg.h>

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
    .mknod = vfs_mknod,
    .ioctl = vfs_ioctl,
};

struct file *new_file(struct vnode *vnode, int flags)
{
    struct file *file = kmalloc(sizeof(struct file));
    file->f_ops = vnode->f_ops;
    file->f_pos = 0;
    file->flags = flags;
    file->vnode = vnode;
    return file;
}

static struct vnode *new_vnode(struct vnode *parent,
            const struct file_operations *fops,
            const char *component_name, uint8_t type)
{
    struct vnode *vn = kmalloc(sizeof(struct vnode));
    memset(vn->component_name, 0, FILE_COMPONENT_NAME_LEN);
    strcpy(vn->component_name, component_name);
    LIST_INIT(vn->list);
    vn->f_ops = fops;
    vn->v_ops = &vnode_ops;
    vn->type = type;
    vn->mount = NULL;
    vn->size = 0;
    vn->cache_attr = 0;
    vn->internal.mem = NULL;

    if (parent == NULL)
        vn->parent = vn;
    else
        vn->parent = parent;
    
    return vn;
}

static int get_root_vnode(const char *pathname, struct vnode **vnode)
{
    if (!memcmp(pathname, "/", 1)) {
        *vnode = rootfs->root;
        return 1;
    }

    *vnode = current->workdir;
    return 0;
}

int __vfs_lookup(const char *pathname, char *component_name,
                struct vnode **prev, struct vnode **target)
{
    int i, j;
    int pathlen = strlen(pathname);

    if (pathlen == 0) {
        *target = *prev;
        return 0;
    }

    i = get_root_vnode(pathname, target);
    for (; i < pathlen; i++) {
        *prev = *target;
        *target = NULL;

        while (pathname[i] == '/') i++;
        for (j = 0; pathname[i] != '/' && i < pathlen; j++, i++)
            component_name[j] = pathname[i];
        component_name[j] = '\0';

        /* If vnode is mounted, update to mounting point */
        if ((*prev)->mount != NULL)
            *prev = (*prev)->mount->root;
        else if ((*prev)->type & FILE_SLINK)
            *prev = (*prev)->internal.soft_link;
        
        (*prev)->v_ops->lookup(*prev, target, component_name);
        if (i != pathlen && *target == NULL)
            return -1;
    }
    return 0;
}

static int is_fs_rdonly(struct vnode *fiter)
{
    while (fiter->parent != fiter)
        fiter = fiter->parent;
    
    return fiter->mount->fs->config & FS_RDONLY;
}

int register_filesystem(const struct filesystem *fs)
{
    if (rootfs != NULL)
        return -1;

    rootfs = kmalloc(sizeof(struct mount));
    rootfs->fs = fs;

    struct vnode *new, *vnode;
    
    vnode = new_vnode(NULL, &file_ops, "/", FILE_DIR);
    vnode->mount = rootfs;
    rootfs->root = vnode;
    
    vnode->v_ops->create(vnode, &new, &file_ops, ".", FILE_SLINK);
    new->internal.soft_link = vnode;
    
    vnode->v_ops->create(vnode, &new, &file_ops, "..", FILE_SLINK);
    new->internal.soft_link = vnode;
    
    fs->setup_mount(fs, rootfs);

    current->workdir = rootfs->root;

    if (vfs_mkdir("/dev") != 0)
        hangon();

    if (vfs_mknod("/dev/uart", &uart_file_ops, NULL) != 0)
        hangon();

    if (vfs_mknod("/dev/framebuffer", &fb_file_ops, mailbox_init) != 0)
        hangon();

    if (vfs_mkdir("/initramfs") != 0)
        hangon();
    
    if (vfs_mount("/initramfs", "initramfs") != 0)
        hangon();

    if (vfs_mkdir("/boot") != 0)
        hangon();

    if (vfs_mount("/boot", "fat32") != 0)
        hangon();

    return 0;
}

int vfs_open(struct vnode *dir_node, struct vnode *vnode,
             const char *component_name, int flags, struct file **target)
{
    if (vnode == NULL) {
        if (!(flags & O_CREAT))
            return -1;
        
        dir_node->v_ops->create(dir_node, &vnode, &file_ops,
                                component_name, FILE_NORM);
    }

    struct file *file = new_file(vnode, flags);
    *target = file;

    return 0;
}

int __vfs_open_wrapper(const char *pathname, int flags, struct file **file)
{
    struct vnode *dir_node, *vnode = NULL;
    char component_name[FILE_COMPONENT_NAME_LEN];

    if (__vfs_lookup(pathname, component_name, &dir_node, &vnode) != 0)
        return -1;

    // if (vnode != NULL)
    //     return vnode->f_ops->open(dir_node, vnode, component_name,
    if (dir_node != NULL)
        return dir_node->f_ops->open(dir_node, vnode, component_name,
                                  flags, file);

    return rootfs->root->f_ops->open(dir_node, vnode, component_name,
                                     flags, file);
}

int vfs_close(struct file *file)
{
    kfree(file);
    return 0;
}

int vfs_write(struct file *file, const void *buf, uint64_t len)
{
    if (!(file->vnode->type & FILE_NORM) || is_fs_rdonly(file->vnode))
        return -1;

    if (file->vnode->internal.mem == NULL) {
        file->vnode->internal.mem = kmalloc(FILE_MAX_SIZE);
        memset(file->vnode->internal.mem, 0, FILE_MAX_SIZE);
    }

    const char *ptr = buf;
    uint64_t i;
    for (i = 0; i < len && file->f_pos < FILE_MAX_SIZE; i++, file->f_pos++)
        *(file->vnode->internal.mem + file->f_pos) = *ptr++;

    if (file->f_pos > file->vnode->size)
        file->vnode->size = file->f_pos;

    return i;
}

int vfs_read(struct file *file, void *buf, uint64_t len)
{
    if (!(file->vnode->type & FILE_NORM) || file->vnode->internal.mem == NULL)
        return -1;

    char *ptr = buf;
    uint64_t i;
    for (i = 0; i < len && file->f_pos < file->vnode->size; i++, file->f_pos++)
        *ptr++ = *(file->vnode->internal.mem + file->f_pos);

    return i;
}

long vfs_lseek64(struct file *file, long offset, int whence)
{
    if (whence == SEEK_SET)
        file->f_pos = offset;
    
    return 0;
}

// ======================================

int vfs_mount(const char *target, const char *filesystem)
{
    const struct filesystem *fs;

    if (!strcmp(filesystem, "tmpfs")) {
        fs = &tmpfs;
    } else if (!strcmp(filesystem, "initramfs")) {
        fs = &initramfs;
    } else if (!strcmp(filesystem, "fat32")) {
        fs = &fat32;
    } else {
        return -1;
    }

    char component_name[FILE_COMPONENT_NAME_LEN];
    struct vnode *dir_node, *vnode, *new;

    if (__vfs_lookup(target, component_name, &dir_node, &vnode) != 0)
        return -1;

    if (vnode == NULL || vnode->mount != NULL || vnode->type != FILE_DIR)
        return -1;

    struct mount *new_mount_point = kmalloc(sizeof(struct mount));
    struct vnode *new_root = new_vnode(NULL, fs->fops, component_name, FILE_DIR);
    
    new_mount_point->fs = fs;
    new_mount_point->root = new_root;
    vnode->mount = new_mount_point;
    new_root->mount = new_mount_point;

    new_root->v_ops->create(new_root, &new, fs->fops, ".", FILE_SLINK);
    new->internal.soft_link = new_root;

    new_root->v_ops->create(new_root, &new, fs->fops, "..", FILE_SLINK);
    new->internal.soft_link = vnode->parent;

    fs->setup_mount(fs, new_mount_point);

    return 0;
}

int vfs_lookup(struct vnode *dir_node, struct vnode **target,
                const char *component_name)
{
    struct vnode *first_file = dir_node->internal.next_layer;
    struct vnode *fiter = first_file;

    if (first_file == NULL)
        return -1;

    do {
        if (!strcmp(fiter->component_name, component_name)) {
            *target = fiter;
            return 0;
        }

        fiter = container_of(fiter->list.next, struct vnode, list);
    } while (fiter != first_file);

    return -1;
}

int vfs_create(struct vnode *dir_node, struct vnode **target,
                const struct file_operations *fops,
                const char *component_name, uint8_t type)
{
    struct vnode *first = dir_node->internal.next_layer;
    struct vnode *vn = new_vnode(dir_node, fops, component_name, type);

    if (first == NULL)
        dir_node->internal.next_layer = vn;
    else 
        list_add_tail(&vn->list, &first->list);

    *target = vn;
    return 0;
}

int vfs_mkdir(const char *pathname)
{
    char component_name[FILE_COMPONENT_NAME_LEN];
    struct vnode *dir_node, *vnode, *new;

    if (__vfs_lookup(pathname, component_name, &dir_node, &vnode) != 0)
        return -1;

    if (vnode != NULL || is_fs_rdonly(dir_node))
        return -1;
    
    if (dir_node->v_ops->create(dir_node, &vnode, &file_ops,
                                component_name, FILE_DIR) != 0)
        return -1;

    vnode->v_ops->create(vnode, &new, &file_ops, ".", FILE_SLINK);
    new->internal.soft_link = vnode;

    vnode->v_ops->create(vnode, &new, &file_ops, "..", FILE_SLINK);
    new->internal.soft_link = vnode->parent;
    
    return 0;
}

int vfs_mknod(const char *pathname, const struct file_operations *fops,
              void (*initfunc)(struct vnode *vn))
{
    char component_name[FILE_COMPONENT_NAME_LEN];
    struct vnode *dir_node, *vnode;

    if (__vfs_lookup(pathname, component_name, &dir_node, &vnode) != 0)
        return -1;

    if (vnode != NULL || is_fs_rdonly(dir_node))
        return -1;

    dir_node->v_ops->create(dir_node, &vnode, fops, component_name, FILE_CHR);

    if (initfunc != NULL)
        initfunc(vnode);

    return 0;
}

int vfs_ioctl(struct file *file, unsigned long request, va_list args)
{
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

    if (__vfs_open_wrapper(pathname, flags, &file) != 0)
        return -1;

    if (file == NULL)
        return -1;
    
    current->fdt->files[fdt_idx] = file;
    return fdt_idx;
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

    if (vnode->type & FILE_SLINK) {
        current->workdir = vnode->internal.soft_link;

        if (vnode->internal.soft_link->mount != NULL)
            current->workdir = vnode->internal.soft_link->mount->root;
    } else {
        current->workdir = vnode;
    }

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
    if ((uint32_t)fd >= FDT_SIZE || current->fdt->files[fd] == NULL)
        return -1;

    int res;
    va_list args;
    
    va_start(args, request);
    res = current->fdt->files[fd]->f_ops->ioctl(current->fdt->files[fd], request, args);
    va_end(args);

    return res;
}

void svc_sync()
{
    fat32_sb_sync();
}