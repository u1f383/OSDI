#include <util.h>
#include <types.h>
#include <vm.h>
#include <mm.h>
#include <sched.h>
#include <printf.h>

#define MMAP_MAX_SIZE 0x10000
#define MMAP_DEFAULT_BASE 0x8787000

static inline vm_area_struct *new_vma(uint64_t vm_start, uint64_t vm_end,
                                      int flags, uint64_t prot, uint64_t attr)
{
    vm_area_struct *vma = kmalloc(sizeof(vm_area_struct));
    vma->vm_start = vm_start;
    vma->vm_end = vm_end;
    vma->flags = flags;
    vma->attr = attr;
    vma->prot = prot;
    vma->data = NULL;
    LIST_INIT(vma->list);
    return vma;
}

static vm_area_struct *insert_vma(mm_struct *mm, uint64_t start, uint64_t end,
                                  int flags, uint64_t prot, uint64_t attr)
{
    vm_area_struct *vma = new_vma(start, end, flags, prot, attr);
    if (mm->mmap == NULL) {
        mm->mmap = vma;
        goto insert_vma_success;
    }

    vm_area_struct *final_vma = container_of(mm->mmap->list.prev, vm_area_struct, list);
    vm_area_struct *vma_iter = final_vma;

    do {
        if (vma->vm_start >= vma_iter->vm_end) {
            list_add(&vma->list, &vma_iter->list);
            goto insert_vma_success;
        }
        
        vma_iter = container_of(vma_iter->list.prev, vm_area_struct, list);
    } while (vma_iter != final_vma);

    list_add_tail(&vma->list, &mm->mmap->list);
    mm->mmap = vma;

insert_vma_success:
    return vma;
}

void release_vma(mm_struct *mm)
{
    vm_area_struct *first_vma = mm->mmap;
    vm_area_struct *vma_iter = container_of(first_vma->list.next, vm_area_struct, list);
    vm_area_struct *prev_vma;

    while (vma_iter != first_vma) {
        prev_vma = vma_iter;
        vma_iter = container_of(vma_iter->list.next, vm_area_struct, list);
        list_del(&prev_vma->list);
        kfree(prev_vma);
    }

    kfree(first_vma);
    mm->mmap = NULL;
}

void mappages(void *pgd, uint64_t va, uint64_t size, uint64_t pa, uint64_t attr)
{
    va &= ~MM_VIRT_KERN_START;
    if (va & PAGE_OFFSET_MASK)
        hangon();

    void *page;
    uint64_t pgcnt = size >> PAGE_SHIFT;
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
        pgtables[level+1] = (void *)phys_to_virt(*((uint64_t *)pgtables[level] + pgtable_idx[level]) & ~ATTR_MASK);
    }
    
    uint8_t update;
    uint64_t pte_pa;
    do {
        pte_pa = *((uint64_t *)pgtables[3] + pgtable_idx[3]) & ~ATTR_MASK;
        if (pte_pa) {
            memcpy((void *)phys_to_virt(pa), (void *)phys_to_virt(pte_pa), PAGE_SIZE);
            buddy_free((void *)phys_to_virt(pte_pa));
        }

        *((uint64_t *)pgtables[3] + pgtable_idx[3]) = pa | BASE_PTE_ATTR | attr;
        update = 0;
        pa += PAGE_SIZE;
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
            pgtables[level+1] = (void *) phys_to_virt(*((uint64_t *)pgtables[level] + pgtable_idx[level]) & ~ATTR_MASK);
        }
    } while (1);
}

pte_t *walk(void *pagetable, uint64_t va)
{
    va &= ~MM_VIRT_KERN_START;

    uint32_t pgtable_idx[4] = {
        va >> PGD_BIT,
        va >> PUD_BIT & ((1 << GRANULE_SIZE) - 1),
        va >> PMD_BIT & ((1 << GRANULE_SIZE) - 1),
        va >> PTE_BIT & ((1 << GRANULE_SIZE) - 1),
    };

    uint64_t ent;
    for (int level = 0; level < 3; level++) {
        ent = *((uint64_t *)pagetable + pgtable_idx[level]);
        if (ent == NULL)
            return NULL;

        if ((ent & 0b11) == PD_BLOCK)
            return (pte_t *)ent;
        
        pagetable = (void *)phys_to_virt(ent & ~ATTR_MASK);
    }

    return *((pte_t **)pagetable + pgtable_idx[3]);
}

void release_pgtable(void *pagetable, int level)
{
    for (int i = 0; i < 512; i++) {
        if (*((uint64_t *)pagetable + i) != NULL) {
            void *page = (void *)phys_to_virt(*((uint64_t *)pagetable + i) & ~ATTR_MASK);
            if (level != 3)
                release_pgtable(page, level+1);
            kfree(page);
        }
    }
}

uint64_t find_vma_start_addr(mm_struct *mm, uint64_t addr, uint64_t len)
{
    vm_area_struct *first_vma = mm->mmap;
    if (first_vma == NULL)
        return addr;

    vm_area_struct *prev_vma = first_vma;
    vm_area_struct *curr_vma = container_of(first_vma->list.next, vm_area_struct, list);

    while (curr_vma != first_vma) {
        if (addr >= prev_vma->vm_end) {
            if (addr + len < curr_vma->vm_start)
                return addr;
            
            if (addr < curr_vma->vm_end)
                addr = curr_vma->vm_end;
        }
        
        prev_vma = curr_vma;
        curr_vma = container_of(curr_vma->list.next, vm_area_struct, list);
    }

    /* Overlap with first vma */
    if (addr <= first_vma->vm_end && addr + len >= first_vma->vm_start)
        addr = first_vma->vm_end;

    return addr;
}

vm_area_struct *find_vma(mm_struct *mm, uint64_t addr)
{
    vm_area_struct *first_vma = mm->mmap;
    vm_area_struct *vma_iter = first_vma;

    do {
        if (addr >= vma_iter->vm_start && addr < vma_iter->vm_end)
            return vma_iter;
        
        vma_iter = container_of(vma_iter->list.next, vm_area_struct, list);
    } while (vma_iter != first_vma);

    return NULL;
}

vm_area_struct *mmap_internal(mm_struct *mm, void* addr, uint64_t len, int prot, int flags)
{
    vm_area_struct *vma;
    uint64_t _addr;
    uint64_t attr;

    attr = PTE_PXN; 
    attr |= prot & PROT_EXEC ? 0 : PTE_UXN;
    
    if (prot & PROT_WRITE)
        attr |= PTE_AP_RDWR;
    else if (prot & PROT_READ)
        attr |= PTE_AP_RDONLY;
    else
        attr |= PTE_AP_NOACCESS;

    _addr = (((uint64_t)addr + PAGE_SIZE - 1) & ~PAGE_OFFSET_MASK);
    len = (len + PAGE_SIZE - 1) & ~PAGE_OFFSET_MASK;

    if (addr == NULL && current != main_task)
        _addr = find_vma_start_addr(mm, MMAP_DEFAULT_BASE, len);
    else
        _addr = find_vma_start_addr(mm, (uint64_t)addr, len);

    vma = insert_vma(mm, _addr, _addr + len, flags, prot, attr);

    if (flags & MAP_POPULATE) {
        for (int i = 0; i < len; i += 0x1000) {
            void *pa = buddy_alloc(1);
            memset(pa, 0, PAGE_SIZE);
            mappages(mm->pgd, vma->vm_start + i, PAGE_SIZE, (uint64_t)pa, attr);
        }
    }

    return vma;
}

void *svc_mmap(void* addr, uint64_t len, int prot, int flags, int fd, int file_offset)
{
    vm_area_struct *vma = mmap_internal(current->mm, addr, len, prot, flags);
    return (void *)vma->vm_start;
}

#define EC_EL0_INSN_FAULT 0b100000
#define EC_EL0_DATA_FAULT 0b100100
#define EC_ELn_INSN_FAULT 0b100001
#define EC_ELn_DATA_FAULT 0b100101

#define ISS_EC_INSN_ABORT(ESR) ((ESR >> 26) == EC_EL0_INSN_FAULT || (ESR >> 26) == EC_ELn_INSN_FAULT)
#define ISS_EC_DATA_ABORT(ESR) ((ESR >> 26) == EC_EL0_DATA_FAULT || (ESR >> 26) == EC_ELn_DATA_FAULT)

#define ISS_WNR_BIT (1 << 6)
#define ISS_WNR_IS_WRITE(ESR) (ESR & ISS_WNR_BIT)
#define ISS_WNR_IS_READ(ESR) (!(ESR & ISS_WNR_BIT))
#define TRAN_FAULT_L0 0b000101
#define PERM_FAULT_L1 0b001101

void do_page_fault(uint64_t far, uint32_t esr)
{
    void *pa;
    mm_struct *mm = current->mm;
    uint64_t addr = far;
    vm_area_struct *vma = find_vma(mm, addr);

    /* Illegal virtual address */
    if (vma == NULL)
        goto segfault;

    /* Wrong memory permission */
    if (ISS_EC_DATA_ABORT(esr)) {
        if (ISS_WNR_IS_WRITE(esr) && !(vma->prot & PROT_WRITE))
            goto segfault;
        if (ISS_WNR_IS_READ(esr) && !(vma->prot & (PROT_WRITE | PROT_READ)))
            goto segfault;
    } else if (ISS_EC_INSN_ABORT(esr) && !(vma->prot & PROT_EXEC))
        goto segfault;

    printf("[Translation fault]: %lx\r\n", addr);

    addr &= ~PAGE_OFFSET_MASK;
    pa = buddy_alloc(1);
    mappages(mm->pgd, addr, PAGE_SIZE, virt_to_phys(pa), vma->attr);

    /**
     * If the memory is .text, we need to copy
     * code into memory 
     */
    if (vma->data != NULL)
        memcpy(pa, vma->data + (addr - vma->vm_start), PAGE_SIZE);
    return;

segfault:
    printf("[Segmentation fault]: Kill Process %d\r\n", current->pid);
    thread_release(current, EXIT_CODE_KILL);
    hangon();
}

void dup_vma(mm_struct *parent_mm, mm_struct *child_mm)
{
    vm_area_struct *first_vma = parent_mm->mmap;
    vm_area_struct *vma_iter = first_vma;
    vm_area_struct *vma;

    do {
        vma = kmalloc(sizeof(vm_area_struct));
        memcpy(vma, vma_iter, sizeof(vm_area_struct));
        LIST_INIT(vma->list);

        if (child_mm->mmap == NULL)
            child_mm->mmap = vma;
        else
            list_add_tail(&vma->list, &child_mm->mmap->list);
        
        vma_iter = container_of(vma_iter->list.next, vm_area_struct, list);
    } while (vma_iter != first_vma);
}

void dup_pages(void *parent, void *child, int level)
{
    void *page;
    void *parent_page;

    for (int i = 0; i < 512; i++) {
        if (*((uint64_t *)parent + i) != NULL) {
            parent_page = (void *)phys_to_virt(*((uint64_t *)parent + i) & ~ATTR_MASK);
            if (level == 3) {
                /* Page table entry */
                *((uint64_t *)child + i) = *((uint64_t *)parent + i);
                *((uint64_t *)parent + i) |= PTE_AP_RDONLY;
                *((uint64_t *)child + i)  |= PTE_AP_RDONLY; /* Set page to read only */
                if (buddy_inc_refcnt(parent_page))
                    hangon();
            } else {
                page = buddy_alloc(1);
                memset(page, 0, PAGE_SIZE);
                *((uint64_t *)child + i) = virt_to_phys(page) | PD_TABLE;
                dup_pages(parent_page, page, level + 1);
            }
        }
    }
}