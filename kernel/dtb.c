#include <init/initramfs.h>
#include <lib/printf.h>
#include <kernel/dtb.h>
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

void parse_dtb(char *dtb, void(*callback)())
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
    int node_type;
    Fdt_node *fdt_node;

    char *prop_name_base = dtb + fdt_hdr->off_dt_strings;
    char *fdt_struct = (char *) fdt_rsv;

    int level = 0;
    uint32_t prop_val_32;
    char spaces[0x100];
    
    memset(spaces, ' ', 0xff);
    spaces[0xff] = '\0';

    while (1)
    {
        node_type = *(uint32_t *) fdt_struct;
        node_type = endian_xchg_32(node_type);
        fdt_struct += 4;
        
        switch (node_type)
        {
        case FDT_BEGIN_NODE:
            level++;

            node_name = fdt_struct;
            fdt_struct += strlen(node_name) + 1;
            fdt_struct = (char *) ALIGN_32(fdt_struct);
            break;
        case FDT_PROP:
            fdt_node = (Fdt_node *) fdt_struct;
            fdt_node->len = endian_xchg_32(fdt_node->len);
            fdt_node->nameoff = endian_xchg_32(fdt_node->nameoff);
            fdt_struct += sizeof(Fdt_node);

            prop_name = prop_name_base + fdt_node->nameoff;
            if (!strcmp(prop_name, "linux,initrd-start"))
            {
                prop_val_32 = (*(uint32_t *) fdt_struct);
                cpio_start = (char *) ((uint64_t) endian_xchg_32(prop_val_32));

                if (callback != NULL)
                    callback();
            }
            if (!strcmp(prop_name, "linux,initrd-end"))
            {
                prop_val_32 = (*(uint32_t *) fdt_struct);
                cpio_end = (char *) ((uint64_t) endian_xchg_32(prop_val_32));
            }
            
            fdt_struct += fdt_node->len;
            fdt_struct = (char *) ALIGN_32(fdt_struct);
            break;

        case FDT_END_NODE:
            level--;
            break;
        case FDT_NOP:
            break;
        case FDT_END:
            goto _ret_parse_dtb;
            break;
        default:
            break;
        }
    }
_ret_parse_dtb:
    printf("OK\r\n");
}

