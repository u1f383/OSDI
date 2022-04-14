#ifndef _INIT_DTB_H_
#define _INIT_DTB_H_

#include <util.h>
#define DTB_MAGIC 0xd00dfeed

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

#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE   0x00000002
#define FDT_PROP       0x00000003
#define FDT_NOP        0x00000004
#define FDT_END        0x00000009

void parse_dtb(void (*func)());

#endif /* _KERNEL_DTB_H_ */