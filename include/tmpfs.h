#ifndef _TMPFS_H_
#define _TMPFS_H_

#include <fs.h>

extern const struct filesystem tmpfs;

int tmpfs_setup_mount(const struct filesystem *fs, struct mount *mount);

#endif /* _TMPFS_H_ */