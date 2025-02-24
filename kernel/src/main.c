#include "klib.h"
#include "serial.h"
#include "vme.h"
#include "cte.h"
#include "loader.h"
#include "fs.h"
#include "proc.h"
#include "timer.h"
#include "dev.h"
#include "pci.h"

void init_user_and_go();

int main() {
  init_gdt();
  init_serial();
  init_fs();
  init_page(); // uncomment me at WEEK3-virtual-memory
  init_cte(); // uncomment me at WEEK2-interrupt
  init_timer(); // uncomment me at WEEK2-interrupt
  init_proc(); // uncomment me at WEEK1-os-start
  init_dev(); // uncomment me at Lab3-1
  init_pci();
  printf("Hello from OS!\n");
  init_user_and_go();
  panic("should never come back");
}

void init_user_and_go() {
  // WEEK3: virtual memory
  proc_t *proc = proc_alloc();
  assert(proc);
  proc->cwd = iopen("/",TYPE_NONE);
  char * argv[2] = {"sh",NULL};
  assert(load_user(proc->pgdir, proc->ctx, "sh", argv) == 0);
  proc_addready(proc);

  // proc_addready(proc);
  // while (1)
  // {
  //   proc_yield();
  //
  // proc_t *kernel = proc_curr();
  // while (1){
  //   proc_t *proc_child;
  //   cli();
  //   while (!(proc_child = proc_findzombie(kernel))){
  //     sti();
  //     proc_yield();
  //   }
  //   printf("i am here\n");
  // }
  proc_t * kernel = proc_curr();
  while (1) {
    proc_t *proc_child;
    // Don't use zombie_sem cause there should always be one process being
    // runnable.
    cli(); // close interrupt first
    while (!(proc_child = proc_findzombie(kernel))) {
      sti();
      proc_yield();
    }

    // printf("kernel huishou pid = %d\n",proc_child->pid);

    if (proc_child->group_leader == proc_child)
      proc_free(proc_child);
    else
      thread_free(proc_child);

    kernel->child_num--;
  }
  
}
