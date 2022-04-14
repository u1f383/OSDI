#include <init/initramfs.h>
#include <lib/printf.h>
#include <init/dtb.h>
#include <util.h>
#include <types.h>

extern addr_t dtb_base, dtb_end;

void show_hdr(Fdt_header *fdt_hdr)
{
    printf("sizeof(Fdt_header): 0x%lxd\r\n", sizeof(Fdt_header));
    printf("magic: 0x%x\r\n", fdt_hdr->magic);
    printf("totalsize: 0x%x\r\n", fdt_hdr->totalsize);
    printf("off_dt_struct: 0x%x\r\n", fdt_hdr->off_dt_struct);
    printf("off_dt_strings: 0x%x\r\n", fdt_hdr->off_dt_strings);
    printf("off_mem_rsvmap: 0x%x\r\n", fdt_hdr->off_mem_rsvmap);
    printf("version: 0x%x\r\n", fdt_hdr->version);
    printf("last_comp_version: 0x%x\r\n", fdt_hdr->last_comp_version);
    printf("boot_cpuid_phys: 0x%x\r\n", fdt_hdr->boot_cpuid_phys);
    printf("size_dt_strings: 0x%x\r\n", fdt_hdr->size_dt_strings);
    printf("size_dt_struct: 0x%x\r\n\n", fdt_hdr->size_dt_struct);
}

static inline void show_space(int sz)
{
    sz *= 2;
    while (sz--)
        printf(" ");
}

void parse_dtb(void(*callback)(char*,char*,char*,int))
{
    if (dtb_base == NULL)
        return;

    Fdt_header *fdt_hdr = (Fdt_header *) dtb_base;
    
    fdt_hdr->magic = endian_xchg_32(fdt_hdr->magic);
    if (fdt_hdr->magic != DTB_MAGIC)
        return;

    fdt_hdr->totalsize = endian_xchg_32(fdt_hdr->totalsize);
    fdt_hdr->off_dt_struct = endian_xchg_32(fdt_hdr->off_dt_struct);
    fdt_hdr->off_dt_strings = endian_xchg_32(fdt_hdr->off_dt_strings);
    fdt_hdr->off_mem_rsvmap = endian_xchg_32(fdt_hdr->off_mem_rsvmap);
    fdt_hdr->version = endian_xchg_32(fdt_hdr->version);
    fdt_hdr->last_comp_version = endian_xchg_32(fdt_hdr->last_comp_version);
    fdt_hdr->boot_cpuid_phys = endian_xchg_32(fdt_hdr->boot_cpuid_phys);
    fdt_hdr->size_dt_strings = endian_xchg_32(fdt_hdr->size_dt_strings);
    fdt_hdr->size_dt_struct = endian_xchg_32(fdt_hdr->size_dt_struct);
    #ifdef DEBUG_DTB
        show_hdr(fdt_hdr);
    #endif /* DEBUG_DTB */

    dtb_end = dtb_base + fdt_hdr->totalsize;

    Fdt_rsv_entry *fdt_rsv = (Fdt_rsv_entry *) (dtb_base + fdt_hdr->off_mem_rsvmap);
    int rsv_num = (fdt_hdr->off_dt_struct - fdt_hdr->off_mem_rsvmap) / sizeof(Fdt_rsv_entry);
    for (int i = 0; i < rsv_num; i++)
    {
        fdt_rsv->address = endian_xchg_64(fdt_rsv->address);
        fdt_rsv->size = endian_xchg_64(fdt_rsv->size);
        #ifdef DEBUG_DTB
            printf("%p: addr: 0x%lx, size: 0x%lx\r\n", fdt_rsv, fdt_rsv->address, fdt_rsv->size);
        #endif /* DEBUG_DTB */
        fdt_rsv++;
    }

    char *node_name = NULL;
    char *prop_name;
    char *chr_prop_value;
    uint32_t uint_prop_value;
    uint32_t prop_len, nameoff;
    uint32_t node_type;

    char *str_block = dtb_base + fdt_hdr->off_dt_strings;
    char *stru_block = dtb_base + fdt_hdr->off_dt_struct;

    int level = 0;
    while (1)
    {
        node_type = endian_xchg_32( *(uint32_t *) stru_block );
        // printf("%d\r\n", node_type);
        stru_block += 4;

        chr_prop_value = prop_name = NULL;
        prop_len = nameoff = uint_prop_value = 0;
        
        switch (node_type) {
        case FDT_BEGIN_NODE:
            node_name = stru_block;

            #ifdef DEBUG_DTB
                show_space(level);
                printf("[ %s ]: \r\n", node_name);
            #endif /* DEBUG_DTB */

            stru_block = PALIGN(stru_block + strlen(stru_block) + 1, 4);
            level++;
            break;

        case FDT_PROP:
            prop_len = endian_xchg_32( *(uint32_t *) stru_block );
            stru_block += 4;

            nameoff = endian_xchg_32( *(uint32_t *) stru_block );
            prop_name = str_block + nameoff;
            stru_block += 4;
            
            chr_prop_value = stru_block;
            #ifdef DEBUG_DTB
                show_space(level);
                printf("Property [ %s ]: \r\n", prop_name, chr_prop_value);
                show_space(level);
                printf("    (str) %s\r\n", chr_prop_value);
                show_space(level);

                printf("    (hex) ");
                for (int i = 0; i < prop_len; i += 4) {
                    uint_prop_value = endian_xchg_32( *(uint32_t *) (stru_block + i) );
                    printf("0x%x ", uint_prop_value);
                }
                printf("\r\n");
            #endif /* DEBUG_DTB */
            
            stru_block = PALIGN(stru_block + prop_len, 4);
            break;

        case FDT_END_NODE:
            level--;
            break;
        case FDT_NOP:
            break;
        case FDT_END:
            goto parse_dtb_ret;
        default:
            while (1);
        }

        if (prop_name && chr_prop_value)
        callback(node_name, prop_name, chr_prop_value, prop_len);
    }
parse_dtb_ret:
    return;
}

