#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define DTB_MAGIC 0xd00dfeed
#define ALIGN_32(ptr) ( (((uint64_t) ptr) + 3) & ~3 )
#define endian_xchg_32(val) ( ((val>>24)&0xff)     | \
                              ((val>>8)&0xff00)    | \
                              ((val<<8)&0xff0000)  | \
                              ((val<<24)&0xff000000) )

#define endian_xchg_64(val) ( ((val>>56)&0xff)               | \
                              ((val>>40)&0xff00)             | \
                              ((val>>24)&0xff0000)           | \
                              ((val>>8)&0xff000000)          | \
                              ((val<<8)&0xff00000000)        | \
                              ((val<<24)&0xff0000000000)     | \
                              ((val<<40)&0xff000000000000)   | \
                              ((val<<56)&0xff00000000000000) )

/**
 * struct fdt_header
 * (free space)
 * memory reservation block
 * (free space)
 * structure block
 * (free space)
 * string block
 */
typedef struct _Fdt_header
{
    uint32_t magic;
    /**
     * Contain the header, the mem rsrvmap,
     * structure block and strings block
     */
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} Fdt_header;

typedef struct _Fdt_rsv_entry
{
    uint64_t address;
    uint64_t size;
} Fdt_rsv_entry;

typedef struct _Fdt_node
{
    uint32_t len;
    uint32_t nameoff;
} Fdt_node;

#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE   0x00000002
#define FDT_PROP       0x00000003
#define FDT_NOP        0x00000004
#define FDT_END        0x00000009

void show_hdr(Fdt_header *fdt_hdr)
{
    printf("sizeof(Fdt_header): 0x%lxd\n", sizeof(Fdt_header));
    printf("magic: 0x%x\n", fdt_hdr->magic);
    printf("totalsize: 0x%x\n", fdt_hdr->totalsize);
    printf("off_dt_struct: 0x%x\n", fdt_hdr->off_dt_struct);
    printf("off_dt_strings: 0x%x\n", fdt_hdr->off_dt_strings);
    printf("off_mem_rsvmap: 0x%x\n", fdt_hdr->off_mem_rsvmap);
    printf("version: 0x%x\n", fdt_hdr->version);
    printf("last_comp_version: 0x%x\n", fdt_hdr->last_comp_version);
    printf("boot_cpuid_phys: 0x%x\n", fdt_hdr->boot_cpuid_phys);
    printf("size_dt_strings: 0x%x\n", fdt_hdr->size_dt_strings);
    printf("size_dt_struct: 0x%x\n\n", fdt_hdr->size_dt_struct);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
        return 1;

    char *dtb = malloc(0x10000);
    Fdt_header *fdt_hdr = (Fdt_header *)dtb;

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1)
        return 1;
    
    read(fd, dtb, 0x10000);
    close(fd);

    fdt_hdr->magic = endian_xchg_32(fdt_hdr->magic);
    if (fdt_hdr->magic != DTB_MAGIC)
        return -1;

    fdt_hdr->totalsize = endian_xchg_32(fdt_hdr->totalsize);
    fdt_hdr->off_dt_struct = endian_xchg_32(fdt_hdr->off_dt_struct);
    fdt_hdr->off_dt_strings = endian_xchg_32(fdt_hdr->off_dt_strings);
    fdt_hdr->off_mem_rsvmap = endian_xchg_32(fdt_hdr->off_mem_rsvmap);
    fdt_hdr->version = endian_xchg_32(fdt_hdr->version);
    fdt_hdr->last_comp_version = endian_xchg_32(fdt_hdr->last_comp_version);
    fdt_hdr->boot_cpuid_phys = endian_xchg_32(fdt_hdr->boot_cpuid_phys);
    fdt_hdr->size_dt_strings = endian_xchg_32(fdt_hdr->size_dt_strings);
    fdt_hdr->size_dt_struct = endian_xchg_32(fdt_hdr->size_dt_struct);
    show_hdr(fdt_hdr);

    Fdt_rsv_entry *fdt_rsv = (Fdt_rsv_entry *)(dtb + fdt_hdr->off_mem_rsvmap);
    int rsv_num = (fdt_hdr->off_dt_struct - fdt_hdr->off_mem_rsvmap) / sizeof(Fdt_rsv_entry);
    for (int i = 0; i < rsv_num; i++)
    {
        fdt_rsv->address = endian_xchg_64(fdt_rsv->address);
        fdt_rsv->size = endian_xchg_64(fdt_rsv->size);
        printf("%p: addr: 0x%llx, size: 0x%llx\n", fdt_rsv, fdt_rsv->address, fdt_rsv->size);
        fdt_rsv++;
    }

    char *node_name;
    char *prop_name;
    int node_type;
    Fdt_node *fdt_node;

    int unknown_node = 0;
    char *prop_name_base = dtb + fdt_hdr->off_dt_strings;
    char *fdt_struct = (char *)fdt_rsv;

    int level = 0;
    char *spaces = malloc(0x100);
    memset(spaces, ' ', 0x100);

    puts("------------ start parsing ------------");
    while (1)
    {
        node_type = *(uint32_t *)fdt_struct;
        node_type = endian_xchg_32(node_type);
        fdt_struct += 4;
        
        switch (node_type)
        {
        case FDT_BEGIN_NODE:
            level++;

            node_name = fdt_struct;
            fdt_struct += strlen(node_name) + 1;
            fdt_struct = (char *)ALIGN_32(fdt_struct);
            printf("%.*s begin node string: %s\n", level*2, spaces, node_name);
            break;
        case FDT_END_NODE:
            level--;
            break;
        case FDT_PROP:
            fdt_node = (Fdt_node *)fdt_struct;
            fdt_node->len = endian_xchg_32(fdt_node->len);
            fdt_node->nameoff = endian_xchg_32(fdt_node->nameoff);
            prop_name = prop_name_base + fdt_node->nameoff;
            printf("%.*s  |- prop node [%s] len=0x%x\n", level*2, spaces, prop_name, fdt_node->len);
            
            fdt_struct += (sizeof(Fdt_node) + fdt_node->len);
            fdt_struct = (char *)ALIGN_32(fdt_struct);
            break;
        case FDT_NOP:
            break;
        case FDT_END:
            puts("-------- FDT_END --------");
            break;
        default:
            unknown_node = 1;
            break;
        }

        if (unknown_node)
            break;
    }



    free(dtb);
}
