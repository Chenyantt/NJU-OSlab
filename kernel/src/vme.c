#include "klib.h"
#include "vme.h"
#include "proc.h"

static TSS32 tss;

void init_gdt() {
  static SegDesc gdt[NR_SEG];
  gdt[SEG_KCODE] = SEG32(STA_X | STA_R,   0,     0xffffffff, DPL_KERN);
  gdt[SEG_KDATA] = SEG32(STA_W,           0,     0xffffffff, DPL_KERN);
  gdt[SEG_UCODE] = SEG32(STA_X | STA_R,   0,     0xffffffff, DPL_USER);
  gdt[SEG_UDATA] = SEG32(STA_W,           0,     0xffffffff, DPL_USER);
  gdt[SEG_TSS]   = SEG16(STS_T32A,     &tss,  sizeof(tss)-1, DPL_KERN);
  set_gdt(gdt, sizeof(gdt[0]) * NR_SEG);
  set_tr(KSEL(SEG_TSS));
}

void set_tss(uint32_t ss0, uint32_t esp0) {
  tss.ss0 = ss0;
  tss.esp0 = esp0;
}

static PD kpd;
static PT kpt[PHY_MEM / PT_SIZE] __attribute__((used));

typedef union free_page {
  union free_page *next;
  char buf[PGSIZE];
} page_t;

page_t *free_page_list;

void init_page() {
  extern char end;
  panic_on((size_t)(&end) >= KER_MEM - PGSIZE, "Kernel too big (MLE)");
  static_assert(sizeof(PTE) == 4, "PTE must be 4 bytes");
  static_assert(sizeof(PDE) == 4, "PDE must be 4 bytes");
  static_assert(sizeof(PT) == PGSIZE, "PT must be one page");
  static_assert(sizeof(PD) == PGSIZE, "PD must be one page");
  // Lab1-4: init kpd and kpt, identity mapping of [0 (or 4096), PHY_MEM)
  int pde_cnt = PHY_MEM / PT_SIZE;
  PT* pt = kpt;
  uint32_t m = 0;
  for(int i = 0; i < pde_cnt; ++i){
    kpd.pde[i].val = MAKE_PDE(pt,1);
    PTE* p = pt->pte;
    for(int j = 0; j < NR_PTE; ++j){
      p->val = MAKE_PTE(m, 1);
      m += PGSIZE;
      ++p;
    }
    ++pt;
  }
  kpt[0].pte[0].val = 0;
  set_cr3(&kpd);
  set_cr0(get_cr0() | CR0_PG);
  // Lab1-4: init free memory at [KER_MEM, PHY_MEM), a heap for kernel
  page_t* fp = (page_t*)KER_MEM;
  while(fp < (page_t*)(PHY_MEM - PGSIZE)){
      page_t* tmp = fp;
      ++fp;
      tmp->next = fp;
  }
  fp->next = NULL;
  free_page_list = (page_t*)KER_MEM;
}

void *kalloc() {
  // Lab1-4: alloc a page from kernel heap, abort when heap empty
  if(free_page_list == NULL) assert(0);
  void* p = free_page_list;

  int pd_index = ADDR2DIR(p);
  PDE *pde = &(vm_curr()->pde[pd_index]); 
  PT *pt = PDE2PT(*pde); 
  int pt_index = ADDR2TBL(p);
  PTE *pte = &(pt->pte[pt_index]);
  pte->present = 1;

  free_page_list = free_page_list->next;
  memset(p, 0, PGSIZE);
  return p;
}

void kfree(void *ptr) {
  // Lab1-4: free a page to kernel heap
  // you can just do nothing :)
  //TODO();
  page_t* p = ptr;
  memset(ptr, 0, PGSIZE);
  p->next = free_page_list;
  free_page_list = p;

  int pd_index = ADDR2DIR(p); 
  PDE *pde = &(vm_curr()->pde[pd_index]); 
  PT *pt = PDE2PT(*pde); 
  int pt_index = ADDR2TBL(p);
  PTE *pte = &(pt->pte[pt_index]);
  pte->present = 0;
  set_cr3(vm_curr());
}

PD *vm_alloc() {
  // Lab1-4: alloc a new pgdir, map memory under PHY_MEM identityly
  PD* pd = kalloc();
  memset(pd, 0, PGSIZE);
  int pde_cnt = PHY_MEM / PT_SIZE;
  PT* pt = kpt;
  for(int i = 0; i < pde_cnt; ++i){
    pd->pde[i].val = MAKE_PDE(pt,1);
    ++pt;
  }
  return pd;
}

void vm_teardown(PD *pgdir) {
  // Lab1-4: free all pages mapping above PHY_MEM in pgdir, then free itself
  // you can just do nothing :)
  //TODO();
  for(int i = PHY_MEM / PT_SIZE; i < NR_PDE; ++i){
    if(pgdir->pde[i].present){
      PT* pt = PDE2PT(pgdir->pde[i]);
      for(int j= 0; j < NR_PTE; ++j){
        if(pt->pte[j].present){
          kfree(PTE2PG(pt->pte[j]));
        }
      }
      kfree(pt);
    }
    
  }
  kfree(pgdir);
}

PD *vm_curr() {
  return (PD*)PAGE_DOWN(get_cr3());
}

PTE *vm_walkpte(PD *pgdir, size_t va, int prot) {
  // Lab1-4: return the pointer of PTE which match va
  // if not exist (PDE of va is empty) and prot&1, alloc PT and fill the PDE
  // if not exist (PDE of va is empty) and !(prot&1), return NULL
  // remember to let pde's prot |= prot, but not pte
  assert((prot & ~7) == 0);
  int pd_index = ADDR2DIR(va);
  PDE *pde = &(pgdir->pde[pd_index]);
  if(pde->present == 0 && (prot & 1)){
    PT* pt = kalloc();
    memset(pt, 0, PGSIZE);
    pde->val = MAKE_PDE(pt, prot);
  }else if(pde->present == 0 && !(prot & 1)){
    return NULL;
  }
  pde->val |= prot;
  PT *pt = PDE2PT(*pde);
  int pt_index = ADDR2TBL(va);
  PTE *pte = &(pt->pte[pt_index]);
  return pte;
}

void *vm_walk(PD *pgdir, size_t va, int prot) {
  // Lab1-4: translate va to pa
  // if prot&1 and prot voilation ((pte->val & prot & 7) != prot), call vm_pgfault
  // if va is not mapped and !(prot&1), return NULL
  PTE* pte = vm_walkpte(pgdir, va, prot);
  if(!pte || !pte->present) return NULL;
  void *page = PTE2PG(*pte);
  void *pa = (void*)((uint32_t)page | ADDR2OFF(va));
  return pa;
}

void vm_map(PD *pgdir, size_t va, size_t len, int prot) {
  // Lab1-4: map [PAGE_DOWN(va), PAGE_UP(va+len)) at pgdir, with prot
  // if have already mapped pages, just let pte->prot |= prot
  assert(prot & PTE_P);
  assert((prot & ~7) == 0);
  size_t start = PAGE_DOWN(va);
  size_t end = PAGE_UP(va + len);
  assert(start >= PHY_MEM);
  assert(end >= start);
  
  for(size_t page = start; page < end; page += PGSIZE){
    PTE* pte = vm_walkpte(pgdir, page, prot);
    if(pte->present){
      uint32_t tmp = pte->val | prot;
      pte->val = MAKE_PTE(PTE2PG(*pte), tmp);
    }
    else{
      void* pa = kalloc();
      pte->val = MAKE_PTE(pa, prot);
    }
  }
}

void vm_unmap(PD *pgdir, size_t va, size_t len) {
  // Lab1-4: unmap and free [va, va+len) at pgdir
  // you can just do nothing :)
  assert(ADDR2OFF(va) == 0);
  assert(ADDR2OFF(len) == 0);
  //TODO();
  for(size_t page = va; page < va + len; page += PGSIZE){
    int pd_index = ADDR2DIR(va);
    PDE *pde = &(pgdir->pde[pd_index]);
    if(pde -> present){
      PT *pt = PDE2PT(*pde); 
      int pt_index = ADDR2TBL(va); 
      PTE *pte = &(pt->pte[pt_index]);
      if(pte->present){
        kfree(PTE2PG(*pte));
        pte->val = 0;
      }
    }
  }
  if(pgdir == vm_curr()) set_cr3(vm_curr());
}

void vm_copycurr(PD *pgdir) {
  // Lab2-2: copy memory mapped in curr pd to pgdir
  // TODO();
  PD* cur = vm_curr();
  for(uint32_t va = PHY_MEM; va < USR_MEM; va += PGSIZE){
    PTE* pte = vm_walkpte(cur, va, 0);
    if(pte && pte->present){
      int prot = pte->val & 0x7;
      vm_map(pgdir, va, PGSIZE, prot);
      uint32_t* pa = vm_walk(pgdir, va, prot);
      memcpy((void*)pa, PTE2PG(*pte), PGSIZE);
    }
  }
}

void vm_pgfault(size_t va, int errcode) {
  printf("pagefault @ 0x%p, errcode = %d\n", va, errcode);
  panic("pgfault");
}
