#include <init/initramfs.h>
#include <lib/printf.h>
#include <init/dtb.h>
#include <util.h>
#include <types.h>

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

void parse_dtb(char *dtb, void(*callback)(int,char*,char*,int))
{
    Fdt_header *fdt_hdr = (Fdt_header *) dtb;
    
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
    show_hdr(fdt_hdr);

    Fdt_rsv_entry *fdt_rsv = (Fdt_rsv_entry *) (dtb + fdt_hdr->off_mem_rsvmap);
    int rsv_num = (fdt_hdr->off_dt_struct - fdt_hdr->off_mem_rsvmap) / sizeof(Fdt_rsv_entry);
    for (int i = 0; i < rsv_num; i++)
    {
        fdt_rsv->address = endian_xchg_64(fdt_rsv->address);
        fdt_rsv->size = endian_xchg_64(fdt_rsv->size);
        printf("%p: addr: 0x%lx, size: 0x%lx\r\n", fdt_rsv, fdt_rsv->address, fdt_rsv->size);
        fdt_rsv++;
    }

    char *node_name;
    char *prop_name;
    char *chr_prop_value;
    uint32_t uint_prop_value;
    int prop_len, nameoff;
    uint32_t node_type;

    char *prop_name_base = dtb + fdt_hdr->off_dt_strings;
    char *stru_block = dtb + fdt_hdr->off_dt_struct;

    int level = 0;

    while (1)
    {
        node_type = endian_xchg_32( *(uint32_t *) stru_block );
        stru_block += 4;

        chr_prop_value = prop_name = NULL;
        prop_len = nameoff = uint_prop_value = 0;
        
        switch (node_type) {
        case FDT_BEGIN_NODE:
            level++;
            node_name = stru_block;
            // show_space(level);
            // printf("[ %s ] \r\n", node_name);

            stru_block = ALIGN_32(stru_block + strlen(stru_block));
            break;

        case FDT_PROP:
            prop_len = endian_xchg_32( *(uint32_t *) stru_block );
            stru_block += 4;

            nameoff = endian_xchg_32( *(uint32_t *) stru_block );
            stru_block += 4;
            
            chr_prop_value = stru_block;
            prop_name = prop_name_base + nameoff;

            // show_space(level);
            // if (!strlen(chr_prop_value)) {
            //     uint_prop_value = endian_xchg_32( *(uint32_t *) chr_prop_value );
            //     printf("Property [ %s ]: 0x%x\r\n", prop_name, uint_prop_value);
            // } else {
            //     printf("Property [ %s ]: %s\r\n", prop_name, chr_prop_value);
            // }
            
            stru_block = ALIGN_32(stru_block + prop_len);
            break;

        case FDT_END_NODE:
            level--;
            break;
        case FDT_NOP:
            break;
        case FDT_END:
            goto parse_dtb_ret;
        }

        if (prop_name && chr_prop_value)
            callback(node_type, prop_name, chr_prop_value, prop_len);
    }
parse_dtb_ret:
    return;
}

