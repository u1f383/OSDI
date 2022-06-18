#include <fat32.h>
#include <sdhost.h>
#include <util.h>
#include <fs.h>
#include <mm.h>
#include <list.h>
#include <printf.h>

static FAT32Meta fat32_meta;

const struct filesystem fat32 = {
    .config = 0,
    .name = "fat32",
    .setup_mount = fat32_setup_mount,
    .fops = &fat32_fops,
};

const struct file_operations fat32_fops = {
    .open = fat32_open,
    .write = fat32_write,
    .read = fat32_read,
    .close = vfs_close,
    .lseek64 = vfs_lseek64,
    .mknod = vfs_mknod,
    .ioctl = vfs_ioctl,
};

struct cache_vnode {
    struct list_head list;
    struct vnode *vnode;
};

struct cache_vnode cache_vnode_head;

static inline uint32_t get_clusN_sector(uint32_t N)
{
    return (N-2) * fat32_meta.sec_per_clus + fat32_meta.db_sector;
}

#define FAT_ENT_EOF (0xfffffff)
static uint32_t fat32_writeback_internal(uint32_t curr_fat_idx, char *data, uint32_t size)
{
    uint32_t curr_fat_sec;
    uint32_t curr_fat_off;
    uint32_t prev_fat_sec;
    uint32_t prev_fat_off;
    uint32_t fat_sec = fat32_meta.fat_sector;
    uint32_t needed_clus = ((size + SECTOR_SIZE - 1) / SECTOR_SIZE) / fat32_meta.sec_per_clus;
    uint32_t curr_clus_num = 0;
    uint32_t first_clus = 0;

    char buf[SECTOR_SIZE];
    char prev_buf[SECTOR_SIZE];

    prev_fat_sec = 0;
    prev_fat_off = 0;

    if (curr_fat_idx != 0)
        first_clus = curr_fat_idx;

    while (curr_clus_num < needed_clus)
    {
        if (curr_fat_idx != 0 && curr_fat_idx != FAT_ENT_EOF) {
            /* Has next entry */
            curr_fat_off = curr_fat_idx % FAT_ENT_PER_SEC;
            curr_fat_sec = curr_fat_idx / FAT_ENT_PER_SEC;
            read_block(fat_sec + curr_fat_sec, buf);
        } else {
            curr_fat_off = prev_fat_off;
            curr_fat_sec = prev_fat_sec;

            while (1) {
                /* Find new FAT ent */
                read_block(fat_sec + curr_fat_sec, buf);
                for (; curr_fat_off < SECTOR_SIZE; curr_fat_off += 4) {
                    if (*(uint32_t *)(buf + curr_fat_off) == 0)
                        /* Found unused entry */
                        break;
                }

                if (curr_fat_off < SECTOR_SIZE)
                    break;
                
                /* Try next sector */
                curr_fat_sec++;
                curr_fat_off = 0;
            }
        }

        if (curr_fat_idx == 0 && curr_clus_num != 0) {
            *(uint32_t *)(prev_buf + prev_fat_off) = curr_fat_sec * FAT_ENT_PER_SEC + curr_fat_off;
            write_block(fat_sec + prev_fat_sec, prev_buf);
        }

        uint32_t fat_ent_idx = (curr_fat_sec * SECTOR_SIZE + curr_fat_off) / 4;
        
        if (first_clus == 0)
            first_clus = fat_ent_idx;

        if (curr_clus_num == needed_clus - 1) {
            /* Last one cluster */
            *(uint32_t *)(buf + curr_fat_off) = FAT_ENT_EOF;
            write_block(fat_sec + curr_fat_sec, buf);
        }

        write_block(get_clusN_sector(fat_ent_idx), data + curr_clus_num * SECTOR_SIZE);

        prev_fat_sec = curr_fat_sec;
        prev_fat_off = curr_fat_off;
        memcpy(prev_buf, buf, SECTOR_SIZE);

        curr_clus_num++;
        curr_fat_idx = *(uint32_t *)(buf + curr_fat_off);
    }

    return first_clus;
}

static inline void add_fat32_cache_vnode(struct vnode *vnode)
{
    struct cache_vnode *cvn = kmalloc(sizeof(struct cache_vnode));
    LIST_INIT(cvn->list);
    cvn->vnode = vnode;
    list_add(&cvn->list, &cache_vnode_head.list);
}

static int32_t fat32_init()
{
    PartitionDesc *part_desc_ptr;
    char buf[BLOCK_SIZE]; /* SECTOR_SIZE == BLOCK_SIZE */

    read_block(MBR_SECTOR, buf);

    if (buf[510] != 0x55 || buf[511] != 0xaa)
        /* Not invalid FAT */
        return 1;

    part_desc_ptr = (PartitionDesc *)&buf[446];
    
    int i;
    for (i = 0; i < 4; i++) {
        if ((part_desc_ptr + i)->boot_indicator == 0x80)
            break;
    }

    if (i == 4)
        return 1;

    if (part_desc_ptr->partition_type_desc != 0xb)
        /* Not a FAT32 */
        return 1;

    memcpy(&fat32_meta.part_desc, part_desc_ptr, sizeof(PartitionDesc));
    read_block(fat32_meta.part_desc.start_sector, buf);

    memcpy(&fat32_meta.OEM_name, buf + 3, 8);
    memcpy(&fat32_meta.bytes_per_sec, buf + 11, 2);
    memcpy(&fat32_meta.sec_per_clus, buf + 13, 1);
    memcpy(&fat32_meta.rsvd_sec_cnt, buf + 14, 2);
    memcpy(&fat32_meta.num_fats, buf + 16, 1);
    memcpy(&fat32_meta.root_ent_cnt, buf + 17, 2);
    memcpy(&fat32_meta.sec_per_trk, buf + 24, 2);
    memcpy(&fat32_meta.total_sec32, buf + 32, 4);
    memcpy(&fat32_meta.fat_size32, buf + 36, 4);
    memcpy(&fat32_meta.root_clus, buf + 40, 2);

    fat32_meta.fat_sector = fat32_meta.rsvd_sec_cnt + fat32_meta.part_desc.start_sector;
    fat32_meta.db_sector = fat32_meta.rsvd_sec_cnt + fat32_meta.fat_size32 * fat32_meta.num_fats + \
                           fat32_meta.part_desc.start_sector;

    LIST_INIT(cache_vnode_head.list);

    return 0;
}


static int32_t fat32_lookup_file(FAT32DirEnt *target, const char *name)
{
    char buf[SECTOR_SIZE];
    char tmp_fname[13];
    int tmp_fname_len;
    FAT32DirEnt *fat32_dir_ent;

    read_block(fat32_meta.db_sector, buf);
    fat32_dir_ent = (FAT32DirEnt *)buf;

    int i, j;
    for (i = 0; i < SECTOR_SIZE / sizeof(FAT32DirEnt); i++) {
        if (strlen(fat32_dir_ent->name) == 0)
            break;

        tmp_fname_len = 0;
        for (j = 0; j < 8 && fat32_dir_ent->name[j] != ' '; j++)
            tmp_fname[tmp_fname_len++] = fat32_dir_ent->name[j];

        tmp_fname[tmp_fname_len++] = '.';
        for (j = 0; j < 3 && fat32_dir_ent->ext[j] != ' '; j++)
            tmp_fname[tmp_fname_len++] = fat32_dir_ent->ext[j];

        tmp_fname[tmp_fname_len] = '\x00';
        
        if (!strcmp(tmp_fname, name)) {
            memcpy(target, fat32_dir_ent, sizeof(FAT32DirEnt));
            return 0;
        }
        
        fat32_dir_ent++;
    }

    return -1;
}

int fat32_setup_mount(const struct filesystem* fs, struct mount* mount)
{
    return fat32_init();
}

int fat32_write(struct file *file, const void *buf, uint64_t len)
{
    const char *ptr = buf;
    uint64_t i;

    if (file->f_pos + len > file->vnode->size) {
        /* Reallocate memory */
        uint32_t new_sz = (file->f_pos + len + SECTOR_SIZE - 1) & ~(SECTOR_SIZE - 1);
        void *tmp = kmalloc(new_sz);
        memset(tmp, 0, new_sz);
        memcpy(tmp, file->vnode->internal.mem, file->vnode->size);
        
        if (file->vnode->internal.mem != NULL)
            kfree(file->vnode->internal.mem);
        
        file->vnode->size = file->f_pos + len;
        file->vnode->internal.mem = tmp;
    }

    for (i = 0; i < len && file->f_pos < file->vnode->size; i++, file->f_pos++)
        *(file->vnode->internal.mem + file->f_pos) = *ptr++;

    file->vnode->cache_attr |= FILE_CACHE_ATTR_DIRTY;
    return i;
}

int fat32_read(struct file *file, void *buf, uint64_t len)
{
    char *ptr = buf;
    uint64_t i;

    for (i = 0; i < len && file->f_pos < file->vnode->size; i++, file->f_pos++)
        *ptr++ = *(file->vnode->internal.mem + file->f_pos);

    return i;
}

int fat32_open(struct vnode *dir_node, struct vnode *vnode,
               const char *component_name, int flags, struct file **target)
{
    FAT32DirEnt fat32_dir_ent;
    int lookup_ret;

    if (vnode == NULL) {
        lookup_ret = fat32_lookup_file(&fat32_dir_ent, component_name);

        if (!(flags & O_CREAT) && lookup_ret != 0)
            return -1;

        dir_node->v_ops->create(dir_node, &vnode, &fat32_fops,
                                component_name, FILE_NORM);
        
        add_fat32_cache_vnode(vnode);

        if (lookup_ret == 0) {
            uint32_t start_sector = get_clusN_sector(fat32_dir_ent.fst_clus_lo + (fat32_dir_ent.fst_clus_hi << 16));
            uint32_t end_sector = start_sector + ((fat32_dir_ent.file_size - 1) / SECTOR_SIZE + 1);

            vnode->size = fat32_dir_ent.file_size;
            vnode->internal.mem = kmalloc((end_sector - start_sector) * SECTOR_SIZE);

            char *ptr = vnode->internal.mem;
            for (int i = start_sector; i < end_sector; i++) {
                read_block(i, ptr);
                ptr += SECTOR_SIZE;
            }
        }
    }

    struct file *file = new_file(vnode, flags);
    *target = file;

    return 0;
}

#define LAST_LON_ENTRY 0x40
void fat32_writeback(struct vnode *vnode)
{
    FAT32DirEnt fat32_dir_ent;
    uint32_t first_clus;
    
    int is_new = fat32_lookup_file(&fat32_dir_ent, vnode->component_name);
    char buf[SECTOR_SIZE];

    if (is_new) {
        FAT32DirEnt *tmp_dir_ptr;
        int total_ent = SECTOR_SIZE / sizeof(FAT32DirEnt);

        read_block(fat32_meta.db_sector, buf);
        tmp_dir_ptr = (FAT32DirEnt *)buf;

        /* Find the empty root directory entry */
        int i, j;
        for (i = 0; i < total_ent; i++) {
            if (tmp_dir_ptr->attr == FAT32_ATTR_LONG_NAME) {
                if (*(uint8_t *)tmp_dir_ptr & LAST_LON_ENTRY)
                    break;
            } else if (strlen(tmp_dir_ptr->name) == 0)
                break;
            
            tmp_dir_ptr++;
        }

        if (i == total_ent) /* First sector no space */
            return;
        
        if (vnode->type & FILE_NORM)
            tmp_dir_ptr->attr = FAT32_ATTR_ARCHIVE;
        else if (vnode->type & FILE_DIR)
            tmp_dir_ptr->attr = FAT32_ATTR_DIRECTORY;

        /* Not important */
        tmp_dir_ptr->crt_date = 0;
        tmp_dir_ptr->ntres = 0;
        tmp_dir_ptr->crt_time_tench = 0;
        tmp_dir_ptr->crt_time = 0;
        tmp_dir_ptr->last_acc_date = 0;
        tmp_dir_ptr->wrt_time = 0;
        tmp_dir_ptr->wrt_date = 0;

        memset(&tmp_dir_ptr->name, ' ', 11);
        for (i = 0; i < 8 && vnode->component_name[i] != '.'; i++)
            tmp_dir_ptr->name[i] = vnode->component_name[i];
        
        /* Find the '.' */
        while (vnode->component_name[i] != '.' && i < strlen(vnode->component_name))
            i++;

        i++;
        j = 0;
        while (j < 3 && i < strlen(vnode->component_name))
            tmp_dir_ptr->ext[j++] = vnode->component_name[i++];

        tmp_dir_ptr->file_size = vnode->size;

        first_clus = fat32_writeback_internal(0, vnode->internal.mem, vnode->size);
        if (first_clus == -1)
            return;

        tmp_dir_ptr->fst_clus_hi = first_clus >> 16;
        tmp_dir_ptr->fst_clus_lo = first_clus & 0xffff;
        
        write_block(fat32_meta.db_sector, buf);
    } else {
        first_clus = (fat32_dir_ent.fst_clus_hi << 16) + fat32_dir_ent.fst_clus_lo;
        fat32_writeback_internal(first_clus, vnode->internal.mem, vnode->size);
    }
}

int fat32_sb_sync()
{
    struct cache_vnode *cvn_iter = container_of(cache_vnode_head.list.next, struct cache_vnode, list);

    while (cvn_iter != &cache_vnode_head) {
        if (VNODE_IS_DIRTY(cvn_iter->vnode)) {
            /* writeback */
            fat32_writeback(cvn_iter->vnode);
            cvn_iter->vnode->cache_attr ^= FILE_CACHE_ATTR_DIRTY;
        }
        cvn_iter = container_of(cvn_iter->list.next, struct cache_vnode, list);
    }

    return 0;
}