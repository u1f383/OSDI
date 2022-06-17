#ifndef _FS_H_
#define _FS_H_

#include <types.h>
#include <list.h>
#include <stdarg.h>

struct file_operations;
struct filesystem;
struct vnode;
struct device;
struct file;
struct mount;

struct file_operations {
    int (*write)(struct file *file, const void *buf, uint64_t len);
    int (*read)(struct file *file, void *buf, uint64_t len);
    int (*open)(struct vnode *dir_node, struct vnode *vnode,
                const char *component_name, int flags, struct file **target);
    int (*close)(struct file *file);
    long (*lseek64)(struct file *file, long offset, int whence);
    int (*mknod)(const char *pathname, const struct file_operations *fops,
                 void (*initfunc)(struct vnode *vn));
    int (*ioctl)(struct file *file, unsigned long request, va_list args);
};

struct vnode_operations {
    int (*lookup)(struct vnode *dir_node, struct vnode **target,
                const char *component_name);
    int (*create)(struct vnode *dir_node, struct vnode **target,
                const struct file_operations *fops,
                const char *component_name, uint8_t type);
    int (*mkdir)(const char *component_name);
};

#define FS_RDONLY (1 << 0)
struct filesystem {
    uint8_t config;
    const char *name;
    int (*setup_mount)(const struct filesystem *fs, struct mount *mount);
    const struct file_operations *fops;
};

#define FILE_NORM    (1 << 0)
#define FILE_DIR     (1 << 1)
#define FILE_CHR     (1 << 2)
#define FILE_SLINK   (1 << 3)
#define FILE_MAX_SIZE 4096
#define FILE_COMPONENT_NAME_LEN 16
#define FILE_CACHE_ATTR_DIRTY (1 << 0)
struct vnode {
    char component_name[FILE_COMPONENT_NAME_LEN];
    struct mount *mount;
    struct vnode *parent;
    const struct vnode_operations *v_ops;
    const struct file_operations *f_ops;
    struct list_head list;
    uint8_t type;
    uint8_t cache_attr;
    uint64_t size;

    union {
        char *mem;
        struct vnode *next_layer;
        struct vnode *soft_link;
        struct device *dev;
    } internal;
};
#define VNODE_IS_DIRTY(vnode) (vnode->cache_attr & FILE_CACHE_ATTR_DIRTY)

struct file {
    const struct file_operations *f_ops;
    struct vnode *vnode;
    uint64_t f_pos;
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

struct file *new_file(struct vnode *vnode, int flags);
int register_filesystem(const struct filesystem *fs);

int vfs_open(struct vnode *dir_node, struct vnode *vnode,
             const char *component_name, int flags, struct file **target);
int __vfs_open_wrapper(const char *pathname, int flags, struct file **file);
int vfs_close(struct file *file);
int vfs_write(struct file *file, const void *buf, uint64_t len);
int vfs_read(struct file *file, void *buf, uint64_t len);
long vfs_lseek64(struct file *file, long offset, int whence);

int vfs_mkdir(const char *pathname);
int vfs_lookup(struct vnode *dir_node, struct vnode **target,
                const char *component_name);
int __vfs_lookup(const char *pathname, char *component_name,
                struct vnode **prev, struct vnode **target);
int vfs_create(struct vnode *dir_node, struct vnode **target,
                const struct file_operations *fops,
                const char *component_name, uint8_t type);

int vfs_mount(const char *target, const char *filesystem);
int vfs_mknod(const char *pathname, const struct file_operations *fops,
              void (*initfunc)(struct vnode *vn));
int vfs_ioctl(struct file *file, unsigned long request, va_list args);

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
void svc_sync();

extern struct mount *rootfs;
extern const struct file_operations file_ops;

#endif /* _FS_H_ */