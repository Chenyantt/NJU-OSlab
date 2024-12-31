#include "klib.h"
#include "cte.h"
#include "proc.h"

#define PROC_NUM 64

static __attribute__((used)) int next_pid = 1;

proc_t pcb[PROC_NUM];
static proc_t *curr = &pcb[0];

void init_proc() {
  // Lab2-1, set status and pgdir
  // Lab2-4, init zombie_sem
  // Lab3-2, set cwd
  pcb[0].status = RUNNING;
  pcb[0].pgdir = vm_curr();
  sem_init(&pcb[0].zombie_sem, 0);
}

proc_t *proc_alloc() {
  // Lab2-1: find a unused pcb from pcb[1..PROC_NUM-1], return NULL if no such one
  for(int i = 0; i < PROC_NUM; ++i){
    if(pcb[i].status == UNUSED){
      pcb[i].pid = next_pid;
      ++next_pid;
      pcb[i].status = UNINIT;
      pcb[i].pgdir = vm_alloc();
      pcb[i].brk = 0;
      pcb[i].kstack = kalloc();
      pcb[i].ctx = &pcb[i].kstack->ctx;
      pcb[i].parent = NULL;
      pcb[i].child_num = 0;
      sem_init(&pcb[i].zombie_sem, 0);
      for(int j = 0; j < MAX_USEM; ++j){
        pcb[i].usems[j] = NULL;
      }
      for(int j = 0; j < MAX_UFILE; ++j){
        pcb[i].files[j] = NULL;
      }
      return &pcb[i];
    }
  }
  return NULL;
  // init ALL attributes of the pcb
}

void proc_free(proc_t *proc) {
  // Lab2-1: free proc's pgdir and kstack and mark it UNUSED
  vm_teardown(proc->pgdir);
  kfree(proc->kstack);
  proc->status = UNUSED;
}

proc_t *proc_curr() {
  return curr;
}

void proc_run(proc_t *proc) {
  proc->status = RUNNING;
  curr = proc;
  set_cr3(proc->pgdir);
  set_tss(KSEL(SEG_KDATA), (uint32_t)STACK_TOP(proc->kstack));
  irq_iret(proc->ctx);
}

void proc_addready(proc_t *proc) {
  // Lab2-1: mark proc READY
  proc->status = READY;
}

void proc_yield() {
  // Lab2-1: mark curr proc READY, then int $0x81
  curr->status = READY;
  INT(0x81);
}

void proc_copycurr(proc_t *proc) {
  // Lab2-2: copy curr proc
  // Lab2-5: dup opened usems
  // Lab3-1: dup opened files
  // Lab3-2: dup cwd
  // TODO();
  proc_t* cur = proc_curr();
  vm_copycurr(proc->pgdir);
  proc->brk = cur->brk;
  proc->kstack->ctx = cur->kstack->ctx;
  proc->kstack->ctx.eax = 0;
  proc->parent = cur;
  cur->child_num++;
  for(int j = 0; j < MAX_USEM; ++j){
        proc->usems[j] = cur->usems[j];
        if(cur->usems[j])
          usem_dup(cur->usems[j]);
  }
  for(int j = 0; j < MAX_UFILE; ++j){
        proc->files[j] = cur->files[j];
        if(cur->files[j])
          fdup(cur->files[j]);
  }
}

void proc_makezombie(proc_t *proc, int exitcode) {
  // Lab2-3: mark proc ZOMBIE and record exitcode, set children's parent to NULL
  // Lab2-5: close opened usem
  // Lab3-1: close opened files
  // Lab3-2: close cwd
  // TODO();
  proc->status = ZOMBIE;
  proc->exit_code = exitcode;
  for(int i = 0; i < PROC_NUM; ++i){
    if(pcb[i].parent == proc)
      pcb[i].parent = NULL;
  }
  if(proc->parent) 
    sem_v(&proc->parent->zombie_sem);
  for(int j = 0; j < MAX_USEM; ++j){
        if(proc->usems[j])
          usem_close(proc->usems[j]);
  }
  for(int j = 0; j < MAX_UFILE; ++j){
        if(proc->files[j])
          fclose(proc->files[j]);
  }
}

proc_t *proc_findzombie(proc_t *proc) {
  // Lab2-3: find a ZOMBIE whose parent is proc, return NULL if none
  // TODO();
  for(int i = 0; i < PROC_NUM; ++i){
    if(pcb[i].parent == proc && pcb[i].status == ZOMBIE)
      return &pcb[i];
  }
  return NULL;
}

void proc_block() {
  // Lab2-4: mark curr proc BLOCKED, then int $0x81
  curr->status = BLOCKED;
  INT(0x81);
}

int proc_allocusem(proc_t *proc) {
  // Lab2-5: find a free slot in proc->usems, return its index, or -1 if none
  // TODO();
  for(int i = 0; i < MAX_USEM; ++i){
    if(proc->usems[i] == NULL)
      return i;
  }
  return -1;
}

usem_t *proc_getusem(proc_t *proc, int sem_id) {
  // Lab2-5: return proc->usems[sem_id], or NULL if sem_id out of bound
  // TODO();
  if(sem_id >= MAX_USEM) 
    return NULL;
  return proc->usems[sem_id];
}

int proc_allocfile(proc_t *proc) {
  // Lab3-1: find a free slot in proc->files, return its index, or -1 if none
  for(int i = 0; i < MAX_UFILE; ++i){
    if(proc->files[i] == NULL)
      return i;
  }
  return -1;
}

file_t *proc_getfile(proc_t *proc, int fd) {
  // Lab3-1: return proc->files[fd], or NULL if fd out of bound
  if(fd >= MAX_UFILE) 
    return NULL;
  return proc->files[fd];
}

void schedule(Context *ctx) {
  // Lab2-1: save ctx to curr->ctx, then find a READY proc and run it
  // TODO();
  curr->ctx = ctx;
  for(int next = (((uint32_t)(curr - pcb)) + 1 ) % PROC_NUM;; next = (next + 1) % PROC_NUM){
    if(pcb[next].status == READY){
      proc_run(&pcb[next]);
    }
  }
}
