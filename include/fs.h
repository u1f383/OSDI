#ifndef _FS_H_
#define _FS_H_

#include <types.h>
#include <list.h>

struct file_operations;
struct filesystem;
struct vnode;
struct file;
struct mount;

struct file_operations {
    int (*write)(struct file *file, const void *buf, uint64_t len);
    int (*read)(struct file *file, void *buf, uint64_t len);
    int (*open)(const char *pathname, int flags, struct file **target);
    int (*close)(struct file *file);
    long (*lseek64)(struct file *file, long offset, int whence);
};

struct vnode_operations {
    int (*lookup)(struct vnode *dir_node, struct vnode **target,
                const char *component_name);
    int (*create)(struct vnode *dir_node, struct vnode **target,
                const char *component_name, uint8_t type);
    int (*mkdir)(const char *component_name);
};

struct filesystem {
    const char *name;
    int (*setup_mount)(const struct filesystem *fs, struct mount *mount);
};

#define FILE_NORM (1 << 0)
#define FILE_DIR  (1 << 1)
#define FILE_CHR  (1 << 2)
#define FILE_MAX_SIZE 4096
#define FILE_COMPONENT_NAME_LEN 16
struct vnode {
    char component_name[FILE_COMPONENT_NAME_LEN];
    struct mount *mount;
    struct vnode *parent;
    const struct vnode_operations *v_ops;
    const struct file_operations *f_ops;
    struct list_head list;
    uint8_t type;
    uint64_t size;

    union
    {
        char *mem;
        struct vnode *next_layer;
    } internal;
};

struct file {
    struct vnode *vnode;
    uint64_t f_pos;
    const struct file_operations *f_ops;
    int flags;
};

struct mount {
    struct vnode *root;
    const struct filesystem *fs;
};

#define FDT_SIZE 0x10
struct fdt_struct {
    struct file *files[FDT_SIZE];
};

int register_filesystem(const struct filesystem *fs);

int vfs_open(const char *pathname, int flags, struct file **target);
int vfs_close(struct file *file);
int vfs_write(struct file *file, const void *buf, uint64_t len);
int vfs_read(struct file *file, void *buf, uint64_t len);
long vfs_lseek64(struct file *file, long offset, int whence);

int vfs_mkdir(const char *pathname);
int vfs_lookup(struct vnode *dir_node, struct vnode **target,
                const char *component_name);
int vfs_create(struct vnode *dir_node, struct vnode **target,
                const char *component_name, uint8_t type);

int vfs_mount(const char *target, const char *filesystem);

#define O_CREAT 00000100
int svc_open(const char *pathname, int flags);
int svc_close(int fd);
long svc_write(int fd, const void *buf, unsigned long count);
long svc_read(int fd, void *buf, unsigned long count);
int svc_mkdir(const char *pathname, unsigned mode);
int svc_mount(const char *src, const char *target, const char *filesystem, unsigned long flags, const void *data);
int svc_chdir(const char *path);
#define SEEK_SET 0
long svc_lseek64(int fd, long offset, int whence);
int svc_ioctl(int fd, unsigned long request, ...);

extern struct mount *rootfs;

#endif /* _FS_H_ */