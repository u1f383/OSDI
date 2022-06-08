#include <util.h>
#include <types.h>
#include <vm.h>
#include <mm.h>

void mappages(void *pgd, uint64_t va, uint64_t size, uint64_t pa)
{
    if (va & 0xfff)
        hangon();

    void *page;
    uint64_t pgcnt;

    pgcnt = size >> PAGE_SHIFT;

    uint32_t pgtable_idx[4] = {
        va >> PGD_BIT,
        va >> PUD_BIT & ((1 << GRANULE_SIZE) - 1),
        va >> PMD_BIT & ((1 << GRANULE_SIZE) - 1),
        va >> PTE_BIT & ((1 << GRANULE_SIZE) - 1),
    };
    void *pgtables[4] = {0};

    pgtables[0] = pgd;
    for (int level = 0; level < 3; level++) {
        if (*((uint64_t *)pgtables[level] + pgtable_idx[level]) == 0) {
            page = buddy_alloc(1);
            memset(page, 0, PAGE_SIZE);
            *((uint64_t *)pgtables[level] + pgtable_idx[level]) = virt_to_phys(page) | PD_TABLE;
        }
        pgtables[level+1] = (void *)phys_to_virt(*((uint64_t *)pgtables[level] + pgtable_idx[level]) & ~0b11);
    }
    
    uint8_t update;
    do {
        *((uint64_t *)pgtables[3] + pgtable_idx[3]) = pa | PTE_ATTR;
        update = 0;
        pa += 0x1000;
        pgcnt--;
        pgtable_idx[3]++;

        if (pgcnt == 0)
            break;

        for (int level = 3; level > 0 && pgtable_idx[level] >= 512; level--) {
            pgtable_idx[level] = 0;
            pgtable_idx[level-1]++;
            update = 1;
        }

        for (int level = 0; level < 3 && update; level++) {
            if (*((uint64_t *)pgtables[level] + pgtable_idx[level]) == 0) {
                page = buddy_alloc(1);
                memset(page, 0, PAGE_SIZE);
                *((uint64_t *)pgtables[level] + pgtable_idx[level]) = virt_to_phys(page) | PD_TABLE;
            }
            pgtables[level+1] = (void *) phys_to_virt(*((uint64_t *)pgtables[level] + pgtable_idx[level]) & ~0b11);
        }
    } while (1);
}

pte_t *walk(void *pagetable, uint64_t va)
{
    uint32_t pgtable_idx[4] = {
        va >> PGD_BIT,
        va >> PUD_BIT & ((1 << GRANULE_SIZE) - 1),
        va >> PMD_BIT & ((1 << GRANULE_SIZE) - 1),
        va >> PTE_BIT & ((1 << GRANULE_SIZE) - 1),
    };

    for (int level = 0; level < 3; level++) {
        pagetable = (void *)phys_to_virt(*((uint64_t *)pagetable + pgtable_idx[level]) & ~0b11);
        if (pagetable == NULL)
            return NULL;
    }

    return *((pte_t **)pagetable + pgtable_idx[3]);
}

void release_pgtable(void *pagetable, int level)
{
    for (int i = 0; i < 512; i++) {
        if (*((uint64_t *)pagetable + i) != NULL) {
            void *page = (void *)phys_to_virt(*((uint64_t *)pagetable + i) & ~0xfff);
            if (level != 3)
                release_pgtable(page, level+1);
            kfree(page);
        }
    }
}