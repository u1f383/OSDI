#ifndef _FAT32_H_
#define _FAT32_H_

#include <types.h>
#include <fs.h>

#define MBR_SECTOR 0
#define SECTOR_SIZE 512 /* same as BLOCK_SIZE */
#define FAT_ENT_PER_SEC (SECTOR_SIZE / 4)

typedef struct PartitionDesc {
    uint64_t boot_indicator : 8;
    uint64_t start_CHS_values : 24;
    uint64_t partition_type_desc : 8;
    uint64_t end_CHS_values : 24;
    uint64_t start_sector : 32; /* VBR (Volume Boot Record) */
    uint64_t partition_size : 32;
} PartitionDesc;

#define FAT32_ATTR_LONG_NAME 0xf
#define FAT32_ATTR_RDONLY 0x1
#define FAT32_ATTR_HIDDEN 0x2
#define FAT32_ATTR_SYSTEM 0x4
#define FAT32_ATTR_VOLUME_ID 0x8
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE 0x20

typedef struct FAT32DirEnt {
    char name[8];
    char ext[3];
    
    uint8_t attr;
    uint8_t ntres;
    uint8_t crt_time_tench;

    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t last_acc_date;
    
    uint16_t fst_clus_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t fst_clus_lo;
    uint32_t file_size;
} FAT32DirEnt;

typedef struct FAT32Meta {
    PartitionDesc part_desc;

    /* FAT 12/16/32 */
    char OEM_name[8];
    uint16_t bytes_per_sec;
    uint8_t sec_per_clus;
    uint16_t rsvd_sec_cnt;
    uint8_t num_fats;
    uint16_t root_ent_cnt;
    uint16_t sec_per_trk;
    uint32_t total_sec32;

    /* FAT 32 */
    uint16_t fat_size32;
    uint16_t root_clus;

    uint32_t fat_sector; /* FAT */
    uint32_t db_sector; /* data block / root dir */
} FAT32Meta;

int fat32_setup_mount(const struct filesystem* fs, struct mount* mount);
int fat32_write(struct file *file, const void *buf, uint64_t len);
int fat32_read(struct file *file, void *buf, uint64_t len);
int fat32_open(struct vnode *dir_node, struct vnode *vnode,
               const char *component_name, int flags, struct file **target);
int fat32_sb_sync();

extern const struct filesystem fat32;
extern const struct file_operations fat32_fops;

#endif /* _FAT32_H_ */
