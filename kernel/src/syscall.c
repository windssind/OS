#include "klib.h"
#include "cte.h"
#include "sysnum.h"
#include "vme.h"
#include "serial.h"
#include "loader.h"
#include "proc.h"
#include "timer.h"
#include "file.h"
#include "network/arp.h"

typedef int (*syshandle_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

extern void *syscall_handle[NR_SYS];

void do_syscall(Context *ctx)
{
  // TODO: WEEK2-interrupt call specific syscall handle and set ctx register
  int sysnum = 0;
  sysnum = ctx->eax;
  uint32_t arg1 = ctx->ebx;
  uint32_t arg2 = ctx->ecx;
  uint32_t arg3 = ctx->edx;
  uint32_t arg4 = ctx->esi;
  uint32_t arg5 = ctx->edi;
  int res;
  if (sysnum < 0 || sysnum >= NR_SYS)
  {
    res = -1;
  }
  else
  {
    res = ((syshandle_t)(syscall_handle[sysnum]))(arg1, arg2, arg3, arg4, arg5);
  }
  ctx->eax = res;
}

int sys_write(int fd, const void *buf, size_t count)
{
  // TODO: rewrite me at Lab3-1
  // printf("write fd = %d\n",fd);
  file_t *file = proc_getfile(proc_curr()->group_leader, fd);
  // printf("get succeess\n");
  if (file == NULL)
  {
    return -1;
  }
  int ret_value = fwrite(file, buf, count);
  // printf("ret_value = %d\n",ret_value);
  return ret_value;
}

int sys_read(int fd, void *buf, size_t count)
{
  // TODO: rewrite me at Lab3-1
  file_t *file = proc_getfile(proc_curr()->group_leader, fd);
  if (file == NULL)
  {
    return -1;
  }
  return fread(file, buf, count);
}

int sys_brk(void *addr)
{
  // TODO: WEEK3-virtual-memory
  proc_t *proc = proc_curr()->group_leader; // uncomment me in WEEK3-virtual-memory
  size_t brk = proc->brk;                   // rewrite me
  size_t new_brk = PAGE_UP(addr);           // rewrite me
  if (brk == 0)
  {
    // uncomment me in WEEK3-virtual-memory
  }
  else if (new_brk > brk)
  {
    vm_map(vm_curr(), brk, new_brk - brk, 7);
  }
  else if (new_brk < brk)
  {
    // can just do nothing
    // vm_unmap(vm_curr(), new_brk, brk - new_brk);
    // recover memory, Lab 1 extend
  }
  proc->brk = new_brk;
  return 0;
}

void sys_sleep(int ticks)
{
  // TODO(); // WEEK2-interrupt
  uint32_t beg_tick = get_tick();
  while (get_tick() - beg_tick <= ticks)
  {
    // sti(); hlt(); cli(); // chage to me in WEEK2-interrupt
    proc_yield(); // change to me in WEEK4-process-api
                  // thread_yield();
  }
  return;
}

int sys_exec(const char *path, char *const argv[])
{
  // TODO(); // WEEK2-interrupt, WEEK3-virtual-memory
  proc_t *proc = proc_curr()->group_leader;

  PD *pgdir = vm_alloc();
  // PD *old_pgdir = proc->pgdir;
  // proc->pgdir = pgdir;
  if (load_user(pgdir, proc->ctx, path, argv) != 0)
  {
    // vm_teardown(pgdir);
    return -1;
  }

  set_cr3(pgdir);
  set_tss(KSEL(SEG_KDATA), (uint32_t)proc->kstack + PGSIZE);
  // vm_teardown(old_pgdir);
  proc->pgdir = pgdir;
  proc_t *ptr = proc->group_leader->thread_group;
  while (ptr != NULL)
  {
    proc_t *free_thread = ptr;
    ptr = ptr->thread_group;
    thread_free(free_thread);
  }

  proc->thread_group = NULL;
  proc->thread_num = 1;
  proc_run(proc);

  return 0;

  // //DEFAULT
  printf("sys_exec is not implemented yet.");
  while (1)
    ;
}

int sys_getpid()
{
  // TODO(); // WEEK3-virtual-memory
  return proc_curr()->tgid;
}

int sys_gettid()
{
  // TODO(); // Lab2-1
  return proc_curr()->pid;
}

void sys_yield()
{
  proc_yield();
}

int sys_fork()
{
  // TODO(); // WEEK4-process-api
  proc_t *new_proc = proc_alloc();
  if (!new_proc)
    return -1;
  proc_copycurr(new_proc);
  proc_addready(new_proc);
  return new_proc->pid;
}

void sys_exit(int status)
{
  // TODO();
  proc_t *curr_proc = proc_curr();
  // 普通线程的调用
  if (curr_proc->pid != curr_proc->tgid)
  {
    --curr_proc->group_leader->thread_num;

    if (curr_proc->detached == 1)
    {
      for (proc_t *ptr = curr_proc->group_leader; ptr->thread_group != NULL; ptr = ptr->thread_group)
      {
        if (ptr->thread_group == curr_proc)
        {
          ptr->thread_group = curr_proc->thread_group;
          break;
        }
      }

      proc_set_kernel_parent(curr_proc);
    }

    proc_makezombie(curr_proc, status);
  }
  else
  {
    while (curr_proc->thread_num > 1)
    { // cv-like operation
      proc_yield();
    }
    proc_t *exit_proc = curr_proc->thread_group;
    while (exit_proc != NULL)
    {
      proc_t *temp = exit_proc->thread_group;
      thread_free(exit_proc);
      exit_proc = temp;
    }

    proc_makezombie(curr_proc, status);
  }
  INT(0x81);
}

void sys_exit_group(int status)
{
  // TODO();
  // WEEK4 process api
  // printf("arrive exit\n");
  proc_t *exit_proc = proc_curr()->thread_group;
  while (exit_proc != NULL)
  {
    proc_t *temp = exit_proc->thread_group;
    thread_free(exit_proc);
    exit_proc = temp;
  }
  proc_makezombie(proc_curr(), status);
  INT(0x81);
  assert(0);
  // while(1) proc_yield();
}

int sys_wait(int *status)
{
  // TODO(); // WEEK4 process api;
  // printf("arrive wait\n");
  proc_t *curr_proc = proc_curr();
  if (curr_proc->group_leader->child_num == 0)
    return -1;
  sem_p(&curr_proc->group_leader->zombie_sem);
  proc_t *zombie_proc = proc_findzombie(curr_proc->group_leader);
  assert(zombie_proc != NULL);
  // while(zombie_proc == NULL) {
  //   proc_yield();
  //   zombie_proc = proc_findzombie(curr_proc);
  // }
  if (status != NULL)
    *status = zombie_proc->exit_code;
  int proc_pid = zombie_proc->pid;
  proc_free(zombie_proc);
  curr_proc->group_leader->child_num--;
  return proc_pid;

  // sys_sleep(250);
  // return 0;
}

int sys_sem_open(int value)
{
  // TODO(); // WEEK5-semaphore
  proc_t *curr_proc = proc_curr()->group_leader;
  int index = proc_allocusem(curr_proc);
  if (index == -1)
    return -1;
  usem_t *temp_usem = usem_alloc(value);
  if (temp_usem == NULL)
    return -1;
  curr_proc->usems[index] = temp_usem;
  return index;
}

int sys_sem_p(int sem_id)
{
  // TODO(); // WEEK5-semaphore
  proc_t *curr_proc = proc_curr()->group_leader;
  usem_t *temp_usem = proc_getusem(curr_proc, sem_id);
  if (temp_usem == NULL)
    return -1;
  sem_p(&temp_usem->sem);
  return 0;
}

int sys_sem_v(int sem_id)
{
  // TODO(); // WEEK5-semaphore
  proc_t *curr_proc = proc_curr()->group_leader;
  usem_t *temp_usem = proc_getusem(curr_proc, sem_id);
  if (temp_usem == NULL)
    return -1;
  sem_v(&temp_usem->sem);
  return 0;
}

int sys_sem_close(int sem_id)
{
  // TODO(); // WEEK5-semaphore
  proc_t *curr_proc = proc_curr()->group_leader;
  usem_t *temp_usem = proc_getusem(curr_proc, sem_id);
  if (temp_usem == NULL)
    return -1;
  --temp_usem->ref;
  curr_proc->usems[sem_id] = NULL;
  return 0;
}

int sys_open(const char *path, int mode)
{
  // TODO(); // Lab3-1
  int free_file_index = proc_allocfile(proc_curr()->group_leader);
  if (free_file_index == -1)
    return -1;
  file_t *open_file = fopen(path, mode, 0);
  if (!open_file)
    return -1;
  proc_curr()->group_leader->files[free_file_index] = open_file;
  return free_file_index;
}

int sys_close(int fd)
{
  // TODO(); // Lab3-1
  printf("child : enter sys_close pid = %d\n", proc_curr()->pid);
  file_t *close_file = proc_getfile(proc_curr()->group_leader, fd);
  if (!close_file)
    return -1;
  fclose(close_file);
  proc_curr()->group_leader->files[fd] = NULL;
  return 0;
}

int sys_dup(int fd)
{
  // TODO(); // Lab3-1

  proc_t *proc = proc_curr()->group_leader;
  int dup_file_index = proc_allocfile(proc);
  if (dup_file_index == -1)
    return -1;

  file_t *dup_file = proc_getfile(proc, fd);
  if (!dup_file)
    return -1;
  proc->files[dup_file_index] = dup_file;
  fdup(dup_file);
  return dup_file_index;
}

uint32_t sys_lseek(int fd, uint32_t off, int whence)
{
  // TODO(); // Lab3-1

  // Week9
  file_t *seek_file = proc_getfile(proc_curr()->group_leader, fd);
  if (!seek_file)
    return -1;
  uint32_t seek_size = fseek(seek_file, off, whence);
  return seek_size;
}

int sys_fstat(int fd, struct stat *st)
{
  // TODO(); // Lab3-1

  file_t *stat_file = proc_getfile(proc_curr()->group_leader, fd);
  if (!stat_file)
    return -1;
  if (stat_file->type == TYPE_FILE)
  {
    inode_t *stat_inode = stat_file->inode;
    st->node = ino(stat_inode);
    st->size = isize(stat_inode);
    st->type = itype(stat_inode);
  }
  else if (stat_file->type == TYPE_DEV)
  {
    st->type = TYPE_DEV;
    st->node = 0;
    st->size = 0;
  }
  else if (stat_file->type == TYPE_PIPE)
  {
    st->type = TYPE_PIPE;
    st->node = 0;
    st->size = stat_file->pipe->full;
  }
  else if (stat_file->type == TYPE_FIFO)
  {
    st->type = TYPE_FIFO;
    st->node = 0;
    st->size = stat_file->pipe->full;
  }
  else
  {

    return -1;
  }

  return 0;
}

int sys_chdir(const char *path)
{
  inode_t *dir = iopen(path, TYPE_NONE);
  if (!dir)
  {
    return -1;
  }
  if (itype(dir) != TYPE_DIR)
  {
    iclose(dir);
    return -1;
  }
  iclose(proc_curr()->group_leader->cwd);
  proc_curr()->group_leader->cwd = dir;
  return 0;
}

int sys_unlink(const char *path)
{
  return iremove(path);
}

// optional syscall

void *sys_mmap()
{
  // TODO();
  for (size_t addr = USR_MEM; addr < VIR_MEM; addr += PGSIZE)
  {
    PTE *pte = vm_walkpte(vm_curr(), addr, 0);
    if (pte && (pte->val & PTE_P))
      continue;
    int prot = PTE_U | PTE_W | PTE_P;
    vm_map(vm_curr(), addr, PGSIZE, prot);
    return (void *)addr;
  }
  return NULL;
}

void sys_munmap(void *addr)
{
  // TODO();
  vm_unmap(vm_curr(), (size_t)addr, PGSIZE);
}

int sys_clone(int (*entry)(void *), void *stack, void *arg, void (*ret_entry)(void))
{
  // TODO();
  // stack -> 栈顶
  proc_t *curr_proc = proc_curr();
  proc_t *new_proc_t = proc_alloc();

  if (new_proc_t == NULL)
    return -1;

  new_proc_t->tgid = curr_proc->tgid;
  new_proc_t->group_leader = curr_proc->group_leader;
  proc_t *ptr = curr_proc->group_leader;
  while (ptr->thread_group != NULL)
  {
    ptr = ptr->thread_group;
  }
  ptr->thread_group = new_proc_t;
  ++curr_proc->group_leader->thread_num;
  new_proc_t->pgdir = curr_proc->group_leader->pgdir;
  // 压入参数
  // ??? 是否需要在一开始就将栈顶上移
  stack -= 4;
  *(void **)stack = arg;
  stack -= 4;
  *(void **)stack = ret_entry;

  // 设置新创建的线程的ctx（上下文）
  new_proc_t->ctx->eip = (uint32_t)entry;
  new_proc_t->ctx->esp = (uint32_t)stack;
  new_proc_t->ctx->cs = USEL(SEG_UCODE);
  new_proc_t->ctx->ds = USEL(SEG_UDATA);
  new_proc_t->ctx->ss = USEL(SEG_UDATA);
  new_proc_t->ctx->eflags = 0x202;

  // 内核栈在proc_alloc时已经创建了 ？？？
  proc_addready(new_proc_t);
  return new_proc_t->pid;
}

int sys_join(int tid, void **retval)
{
  // TODO();
  proc_t *joining = proc_curr();
  proc_t *joined = pid2proc(tid);
  if (!joined)
    return 3;

  if (joined == joining || joined->joinable != 1)
    return 3;
  joined->joinable = 0;

  sem_p(&joined->join_sem);

  if (retval != NULL)
    *(int *)retval = joined->exit_code;

  return 0;
}

int sys_detach(int tid)
{
  // TODO();
  return thread_detached(tid);
}

int sys_kill(int pid, int signo)
{
  // // TODO();
  // proc_t *kill_proc = pid2proc(pid);

  // if (kill_proc == NULL)
  //   return -1;
  // bool status = kill_proc->status == RUNNING;

  // while (kill_proc->thread_num > 1)
  // { // cv-like operation
  //   proc_yield();
  // }

  // proc_t *exit_proc = kill_proc->thread_group;
  // while (exit_proc != NULL)
  // {
  //   proc_t *temp = exit_proc->thread_group;
  //   thread_free(exit_proc);
  //   exit_proc = temp;
  // }

  // proc_makezombie(kill_proc, 9);

  // if (status)
  //   INT(0x81);

  // return 0;
  proc_t *proc_target = pid2proc(pid);
  if (proc_target == NULL)
  {
    printf("No such process\n");
    return 3;
  }

  if (signo < 0 || signo >= SIGNAL_NUM)
  {
    printf("Invalid signal\n");
    return 22;
  }

  if (signo == SIGSTOP || signo == SIGCONT || signo == SIGKILL)
  {
    proc_target->sigaction[signo](signo, proc_target);
  }
  else
  {
    // 可以缓一下

    // 查看list中是否有相同类型的信号，如果有，直接返回
    list_t *head = &proc_target->sigpending_queue;
    list_t *ptr = head;
    while (ptr != head)
    {
      if (*(int *)ptr == signo)
      {
        return 0;
      }
    }
    // 没有，就挂上去
    list_enqueue(&proc_target->sigpending_queue, (void *)signo);
  }
  return 0;
}

int sys_cv_open()
{
  // TODO();
  return sys_sem_open(0);
}

int sys_cv_wait(int cv_id, int sem_id)
{
  // TODO();
  sys_sem_v(sem_id);
  sys_sem_p(cv_id);
  return 0;
}

int sys_cv_sig(int cv_id)
{
  // TODO();
  return sys_sem_v(cv_id);
}

int sys_cv_sigall(int cv_id)
{
  // TODO();
  usem_t *usem = proc_getusem(proc_curr(), cv_id);
  if (!usem)
    return -1;
  while (usem->sem.value != 0)
  {
    sem_v(&usem->sem);
  }
  return 0;
}

int sys_cv_close(int cv_id)
{
  // TODO();
  return sys_sem_close(cv_id);
}

int sys_pipe(int fd[2])
{
  file_t *fd_pipe[2];
  if (pipe_open(fd_pipe) < 0)
  {
    return -1;
  }
  assert(fd_pipe[0]);
  assert(fd_pipe[1]);
  int read, write;
  // printf("sys_pipe addr: %p\n",proc_curr()->group_leader);
  read = proc_allocfile(proc_curr()->group_leader);
  proc_curr()->group_leader->files[read] = fd_pipe[0];
  fd[0] = read;
  write = proc_allocfile(proc_curr()->group_leader);
  proc_curr()->group_leader->files[write] = fd_pipe[1];
  fd[1] = write;
  return 0;
}

int sys_mkfifo(const char *path, int mode)
{
  if (mkfifo(path, mode) == NULL)
  {
    return -1;
  }
  return 0;
}

int sys_link(const char *oldpath, const char *newpath)
{
  return flink(oldpath, newpath);
}

int sys_symlink(const char *oldpath, const char *newpath)
{
  return fsymlink(oldpath, newpath);
}

int sys_sigaction(int signo, const void *act, void **oldact)
{
  // WEEK8-signal: set new signal action handler
  if (signo < 0 || signo >= SIGNAL_NUM)
  {
    return -1;
  }
  proc_t *proc = proc_curr();
  proc->sigaction[signo] = act;
  if (oldact != NULL)
  {
    *oldact = proc->sigaction[signo];
  }
  return 0;
}

int sys_sigprocmask(int how, const int set, int *oldset)
{
  // WEEK8-signal: set new signal action handler
  proc_t *proc = proc_curr();
  if (oldset != NULL)
  {
    *oldset = proc->sigblocked;
  }
  if (how == SIG_BLOCK)
  {
    proc->sigblocked |= set;
  }
  else if (how == SIG_UNBLOCK)
  {
    proc->sigblocked &= ~set;
  }
  else if (how == SIG_SETMASK)
  {
    proc->sigblocked = set;
  }
  else
  {
    printf("not a valid sigpromask\n");
  }
  return 0;
}

int sys_arp_create(char *interface, char *ipAddr, char *arpResp, size_t size)
{
  // TODO(); // WEEK12-network
  assert(ipAddr);
  if (send_arpRequest(interface, ipAddr, arpResp) < 0)
    return -1;

  return 0;
}

int sys_arp_serve(char *interface)
{
  // TODO(); // WEEK12-network
  return recv_arpRequest(interface);
}

int sys_arp_receive(char *buff, size_t size)
{
  // TODO(); // WEEK12-network
  int return_size = e1000_receive(buff, size);
  if (return_size == 0)
    return -1;
  return return_size;
}

void *syscall_handle[NR_SYS] = {
    [SYS_write] = sys_write,
    [SYS_read] = sys_read,
    [SYS_brk] = sys_brk,
    [SYS_sleep] = sys_sleep,
    [SYS_exec] = sys_exec,
    [SYS_getpid] = sys_getpid,
    [SYS_gettid] = sys_gettid,
    [SYS_yield] = sys_yield,
    [SYS_fork] = sys_fork,
    [SYS_exit] = sys_exit,
    [SYS_exit_group] = sys_exit_group,
    [SYS_wait] = sys_wait,
    [SYS_sem_open] = sys_sem_open,
    [SYS_sem_p] = sys_sem_p,
    [SYS_sem_v] = sys_sem_v,
    [SYS_sem_close] = sys_sem_close,
    [SYS_open] = sys_open,
    [SYS_close] = sys_close,
    [SYS_dup] = sys_dup,
    [SYS_lseek] = sys_lseek,
    [SYS_fstat] = sys_fstat,
    [SYS_chdir] = sys_chdir,
    [SYS_unlink] = sys_unlink,
    [SYS_mmap] = sys_mmap,
    [SYS_munmap] = sys_munmap,
    [SYS_clone] = sys_clone,
    [SYS_join] = sys_join,
    [SYS_detach] = sys_detach,
    [SYS_kill] = sys_kill,
    [SYS_cv_open] = sys_cv_open,
    [SYS_cv_wait] = sys_cv_wait,
    [SYS_cv_sig] = sys_cv_sig,
    [SYS_cv_sigall] = sys_cv_sigall,
    [SYS_cv_close] = sys_cv_close,
    [SYS_pipe] = sys_pipe,
    [SYS_mkfifo] = sys_mkfifo,
    [SYS_link] = sys_link,
    [SYS_symlink] = sys_symlink,
    [SYS_sigaction] = sys_sigaction,
    [SYS_sigprocmask] = sys_sigprocmask,
    [SYS_arp_create] = sys_arp_create,
    [SYS_arp_serve] = sys_arp_serve,
    [SYS_arp_receive] = sys_arp_receive,

    // [SYS_spinlock_open] = sys_spinlock_open,
    // [SYS_spinlock_acquire] = sys_spinlock_acquire,
    // [SYS_spinlock_release] = sys_spinlock_release,
    // [SYS_spinlock_close] = sys_spinlock_close,
};
