

#include "klib.h"
#include "vme.h"
#include "proc.h"

static TSS32 tss;

void init_gdt() {
    static SegDesc gdt[NR_SEG];
    gdt[SEG_KCODE] = SEG32(STA_X | STA_R, 0, 0xffffffff, DPL_KERN);
    gdt[SEG_KDATA] = SEG32(STA_W, 0, 0xffffffff, DPL_KERN);
    gdt[SEG_UCODE] = SEG32(STA_X | STA_R, 0, 0xffffffff, DPL_USER);
    gdt[SEG_UDATA] = SEG32(STA_W, 0, 0xffffffff, DPL_USER);
    gdt[SEG_TSS] = SEG16(STS_T32A, &tss, sizeof(tss) - 1, DPL_KERN);
    set_gdt(gdt, sizeof(gdt[0]) * NR_SEG);
    set_tr(KSEL(SEG_TSS));
}

void set_tss(uint32_t ss0, uint32_t esp0) {
    tss.ss0 = ss0;
    tss.esp0 = esp0;
}

static PD kpd;
static PT kpt[PHY_MEM / PT_SIZE] __attribute__((used));
static size_t pm_ref[PHY_MEM / PGSIZE];

typedef union free_page {
    union free_page *next;
    char buf[PGSIZE];
} page_t;

page_t *free_page_list;

// WEEK3-virtual-memory

void init_page() {
    extern char end;
    panic_on((size_t)(&end) >= KER_MEM - PGSIZE, "Kernel too big (MLE)");
    static_assert(sizeof(PTE) == 4, "PTE must be 4 bytes");
    static_assert(sizeof(PDE) == 4, "PDE must be 4 bytes");
    static_assert(sizeof(PT) == PGSIZE, "PT must be one page");
    static_assert(sizeof(PD) == PGSIZE, "PD must be one page");

    // WEEK3-virtual-memory: init kpd and kpt, identity mapping of [0 (or 4096), PHY_MEM)
    for (int i = 0; i < PHY_MEM / PT_SIZE; i++) {
        kpd.pde[i].val = MAKE_PDE(kpt[i].pte, PTE_P);
        for (int j = 0; j < NR_PTE; j++) {
            kpt[i].pte[j].val = MAKE_PTE(((i << DIR_SHIFT) | (j << TBL_SHIFT)), PTE_P);
        }
    }

    kpt[0].pte[0].val = 0;

    // TODO:check
    // WEEK12网卡: init kpd and kpt, identity mapping of [0 (or 4096), PHY_MEM)
    for (int i = VIR_MEM / PT_SIZE; i < MMIO_MEM  / PT_SIZE; i++) {
        kpd.pde[i].val = MAKE_PDE(kpt[i].pte, PTE_P);
        for (int j = 0; j < NR_PTE; j++) {
            kpt[i].pte[j].val = MAKE_PTE(((i << DIR_SHIFT) | (j << TBL_SHIFT)), PTE_P);
        }
    }

    // WEEK3-virtual-memory: init free memory at [KER_MEM, PHY_MEM), a heap for kernel

    free_page_list = (page_t *)KER_MEM;
    page_t *cur_page = free_page_list;
    page_t *next_page;

    for (size_t addr = KER_MEM + PGSIZE; addr < PHY_MEM; addr += PGSIZE) {
        next_page = (page_t *)addr;
        cur_page->next = next_page;
        cur_page = next_page;
    }

    cur_page->next = NULL;

    for (int i = 0; i < PHY_MEM / PGSIZE; ++i) {
        pm_ref[i] = 0;
    }

    set_cr3(&kpd);
    set_cr0(get_cr0() | CR0_PG);
}

void *kalloc() {
    // WEEK3-virtual-memory: alloc a page from kernel heap, abort when heap empty
    assert(free_page_list);

    page_t *alloc_page = free_page_list;
    free_page_list = free_page_list->next;
    kpt[ADDR2DIR(alloc_page)].pte[ADDR2TBL(alloc_page)].present = 1;

    size_t page_index = (size_t)alloc_page / PGSIZE;
    pm_ref[page_index] = 1;
    return alloc_page;
}

void kfree(void *ptr) {
    // // WEEK3-virtual-memory: free a page to kernel heap
    // // you can just do nothing :)
    // // TODO();
    // page_t *kfree_page = (page_t *)PAGE_DOWN(ptr);

    // size_t page_index = (size_t)kfree_page / PGSIZE;
    // assert(pm_ref[page_index] > 0);
    // pm_ref[page_index]--;

    // if (pm_ref[]) {
    //     kfree_page->next = free_page_list;
    //     free_page_list = kfree_page;
    //     PTE *freed_entry = &kpt[ADDR2DIR(kfree_page)].pte[ADDR2TBL(kfree_page)];
    //     assert(freed_entry->present == 1);
    //     set_cr3(vm_curr());
    // }
}

PD *vm_alloc() {
    // WEEK3-virtual-memory: alloc a new pgdir, map memory under PHY_MEM identityly
    // TODO();
    PD *new_padir = (PD *)kalloc();

    for (int i = 0; i < PHY_MEM / PT_SIZE; i++) {
        new_padir->pde[i].val = MAKE_PDE(&kpt[i], PTE_P);
    }

    for (int i = PHY_MEM / PT_SIZE; i < NR_PDE; i++) {
        new_padir->pde[i].val = 0;
    }

    for (int i = VIR_MEM / PT_SIZE; i < MMIO_MEM  / PT_SIZE; i++) {
        new_padir->pde[i].val = MAKE_PDE(kpt[i].pte, PTE_P);
        for (int j = 0; j < NR_PTE; j++) {
            kpt[i].pte[j].val = MAKE_PTE(((i << DIR_SHIFT) | (j << TBL_SHIFT)), PTE_P);
        }
    }
    return new_padir;
}

void vm_teardown(PD *pgdir) {
    // // WEEK3-virtual-memory: free all pages mapping above PHY_MEM in pgdir, then free itself
    // // you can just do nothing :)
    // // TODO();
    // for (int i = PHY_MEM / PT_SIZE; i < NR_PDE; i++) {
    //     PDE pde = pgdir->pde[i];
    //     if (pde.val == 0) continue;
    //     PT *pt = (PT *)PDE2PT(pde);
    //     if (!pt) continue;
    //     for (int j = 0; j < NR_PTE; j++) {
    //         // if(pt->pte[j].present == 0) continue;
    //         page_t *page = PTE2PG(pt->pte[j]);
    //         if (!page) continue;
    //         kfree(page);
    //     }
    //     kfree(pt);
    // }
    // kfree(pgdir);
}

PD *vm_curr() {
    return (PD *)PAGE_DOWN(get_cr3());
}

PTE *vm_walkpte(PD *pgdir, size_t va, int prot) {
    // WEEK3-virtual-memory: return the pointer of PTE which match va
    // if not exist (PDE of va is empty) and prot&1, alloc PT and fill the PDE
    // if not exist (PDE of va is empty) and !(prot&1), return NULL
    // remember to let pde's prot |= prot, but not pte
    assert((prot & ~7) == 0);
    int pd_index = ADDR2DIR(va);
    PDE *pde = &(pgdir->pde[pd_index]);
    PT *pt = PDE2PT(*pde);

    if (pt == 0) {
        if (prot & 1) {
            pt = (PT *)kalloc();
            pde->val = MAKE_PDE(pt, prot);
            memset(pt, 0, PGSIZE);
        } else
            return NULL;
    }

    if (prot != 0) pde->val |= prot;
    int pt_index = ADDR2TBL(va);
    PTE *pte = &(pt->pte[pt_index]);
    return pte;
}

void *vm_walk(PD *pgdir, size_t va, int prot) {
    // WEEK3-virtual-memory: translate va to pa
    // if prot&1 and prot voilation ((pte->val & prot & 7) != prot), call vm_pgfault
    // if va is not mapped and !(prot&1), return NULL
    assert(va >= PHY_MEM);
    int pd_index = ADDR2DIR(va);
    PDE *pde = &(pgdir->pde[pd_index]);
    PT *pt = PDE2PT(*pde);
    // PTE * pte = vm_walkpte(pgdir, va, prot);
    if (pt == 0) return NULL;
    // if((prot & 1) && ((pte->val & prot & 7) != prot)) {
    //   vm_pgfault(va, prot);

    // }
    int pt_index = ADDR2TBL(va);
    PTE *pte = &(pt->pte[pt_index]);
    void *page = PTE2PG(*pte);
    void *pa = (void *)((uint32_t)page | ADDR2OFF(va));
    return pa;
}

void vm_map(PD *pgdir, size_t va, size_t len, int prot) {
    // WEEK3-virtual-memory: map [PAGE_DOWN(va), PAGE_UP(va+len)) at pgdir, with prot
    // if have already mapped pages, just let pte->prot |= prot
    assert(prot & PTE_P);
    assert((prot & ~7) == 0);
    size_t start = PAGE_DOWN(va);
    size_t end = PAGE_UP(va + len);
    assert(end >= start);

    for (size_t addr = start; addr < end; addr += PGSIZE) {
        PTE *pte = vm_walkpte(pgdir, addr, prot);
        assert(pte);
        pte->val = MAKE_PTE(kalloc(), prot);
        pte->cow = pte->read_write;
    }
}

void vm_unmap(PD *pgdir, size_t va, size_t len) {
    // WEEK3-virtual-memory: unmap and free [va, va+len) at pgdir
    // you can just do nothing :)
    assert(ADDR2OFF(va) == 0);
    assert(ADDR2OFF(len) == 0);
    size_t start = PAGE_DOWN(va);
    size_t end = PAGE_UP(va + len);
    assert(end >= start);

    for (size_t addr = start; addr < end; addr += PGSIZE) {
        PTE *pte = vm_walkpte(pgdir, addr, 0);

        assert(pte);
        void *phy_page = (void *)(PTE2PG(*pte));
        kfree(phy_page);
        pte->val = 0;
    }

    if (pgdir == vm_curr()) {
        set_cr3(vm_curr());
    }
}

void vm_copycurr(PD *pgdir) {
    // WEEK4-process-api: copy memory mapped in curr pd to pgdir
    PD *curr_pgdir = vm_curr();

    for (size_t addr = PHY_MEM; addr < USR_MEM; addr += PGSIZE) {
        PTE *pte = vm_walkpte(curr_pgdir, addr, 0);
        if (pte == NULL || pte->val == 0)
            continue;
        PTE *new_pte = vm_walkpte(pgdir, addr, 7);

        pte->read_write = 0;
        new_pte->val = pte->val;

        pm_ref[pte->page_frame]++;
    }

    for (size_t addr = USR_MEM; addr < VIR_MEM; addr += PGSIZE) {
        PTE *pte = vm_walkpte(curr_pgdir, addr, 0);

        if (pte == NULL || pte->val == 0) continue;

        void *phy_page = PTE2PG(*pte);
        if (phy_page == NULL) continue;

        if (pm_ref[pte->page_frame] == 0) continue;

        pm_ref[pte->page_frame]++;

        int prot = pte->val & 7;

        PTE *new_pte = vm_walkpte(pgdir, addr, prot);
        assert(new_pte);
        new_pte->val = MAKE_PTE(phy_page, prot);
    }
}

void vm_pgfault(size_t va, int errcode) {
    if (errcode & 2) {
        PD *pgdir = vm_curr();
        PTE *pte = vm_walkpte(pgdir, va, 0);
        if (!pte || !(pte->cow)) goto bad;

        pte->val |= PTE_W;
        void *phy_page = vm_walk(pgdir, PAGE_DOWN(va), 0);
        assert(phy_page);

        size_t page_index = (size_t)phy_page / PGSIZE;
        assert(pm_ref[page_index] > 0);
        if (pm_ref[page_index] > 1) {
            pm_ref[page_index]--;
            pte->val |= (PTE_U);

            void *new_phy_page = kalloc();
            assert(new_phy_page);

            memcpy(new_phy_page, phy_page, PGSIZE);
            pte->val = MAKE_PTE(new_phy_page, pte->val & 7);
            pte->cow = pte->read_write;
        }

    } else {
        goto bad;
    }
    set_cr3(vm_curr());
    return;

bad:
    printf("pagefault @ 0x%p, errcode = %d\n", va, errcode);
    panic("pgfault");
}

void *mmio_map_region(uint32_t pa, uint32_t size){ 
  // WEEK12-network
  // Reserve size bytes in the DEVSPACE region. 
  // Return the base of the reserved region.  size does *not*
  // have to be multiple of PGSIZE. 
  return (void*)pa;
}

