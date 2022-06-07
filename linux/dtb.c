#include <dtb.h>
#include <util.h>
#include <types.h>
#include <mm.h>

static uint64_t dtb_base, dtb_end;

void dtb_init(void *_dtb_base)
{
    Fdt_header *fdt_hdr = (Fdt_header *)_dtb_base;
    dtb_base = (uint64_t)_dtb_base + MM_VIRT_KERN_START;
    dtb_end = dtb_base + endian_xchg_32(fdt_hdr->totalsize);
    register_mem_reserve(dtb_base, dtb_end);
}

void parse_dtb(int(*callback)(char*, char*, char*, int))
{
    if (dtb_base == NULL)
        return;

    Fdt_header *fdt_hdr = (Fdt_header *)dtb_base;
    
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

    Fdt_rsv_entry *fdt_rsv = (Fdt_rsv_entry *)(dtb_base + fdt_hdr->off_mem_rsvmap);
    int rsv_num = (fdt_hdr->off_dt_struct - fdt_hdr->off_mem_rsvmap) / sizeof(Fdt_rsv_entry);
    for (int i = 0; i < rsv_num; i++) {
        fdt_rsv->address = endian_xchg_64(fdt_rsv->address);
        fdt_rsv->size = endian_xchg_64(fdt_rsv->size);
        fdt_rsv++;
    }

    char *node_name;
    char *prop_name;
    void *prop_value_ptr;
    uint32_t prop_len, nameoff;
    uint32_t node_type;

    char *str_block = (char *)dtb_base + fdt_hdr->off_dt_strings;
    char *stru_block = (char *)dtb_base + fdt_hdr->off_dt_struct;

    while (1)
    {
        node_type = endian_xchg_32(*(uint32_t *)stru_block);
        stru_block += 4;

        prop_value_ptr = prop_name = NULL;
        prop_len = nameoff = 0;
        
        switch (node_type) {
        case FDT_BEGIN_NODE:
            node_name = stru_block;
            stru_block = PALIGN(stru_block + strlen(stru_block) + 1, 4);
            break;

        case FDT_PROP:
            prop_len = endian_xchg_32(*(uint32_t *)stru_block);
            stru_block += 4;

            nameoff = endian_xchg_32(*(uint32_t *)stru_block);
            prop_name = str_block + nameoff;
            stru_block += 4;
            
            prop_value_ptr = stru_block;
            stru_block = PALIGN(stru_block + prop_len, 4);
            break;

        case FDT_END_NODE:
            break;

        case FDT_NOP:
            break;

        case FDT_END:
            goto parse_dtb_ret;

        default:
            hangon();
        }

        if (callback(node_name, prop_name, prop_value_ptr, prop_len) == 1)
            return;
    }

parse_dtb_ret:
    return;
}

