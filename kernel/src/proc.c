// #include "klib.h"
// #include "cte.h"
// #include "proc.h"

// #define PROC_NUM 64

// static __attribute__((used)) int next_pid = 1;

// proc_t pcb[PROC_NUM];
// static proc_t *curr = &pcb[0];

// proc_t *pid2proc(int pid)
// {
//     for (int i = 0; i < PROC_NUM; ++i)
//     {
//         if (pcb[i].pid == pid)
//             return &pcb[i];
//     }

//     return NULL;
// }

// void init_proc()
// {
//     // WEEK1: init proc status
//     pcb[0].status = RUNNING;
//     pcb[0].pid = 0;
//     // WEEK2: add ctx and kstack for interruption
//     pcb[0].kstack = (void *)(KER_MEM - PGSIZE);
//     pcb[0].ctx = &pcb[0].kstack->ctx;
//     // WEEK3: add pgdir
//     pcb[0].pgdir = vm_curr();
//     pcb[0].parent = NULL;
//     pcb[0].child_num = 0;
//     // WEEK5: semaphore
//     sem_init(&pcb[0].zombie_sem, 0);
//     for (int j = 0; j < MAX_USEM; j++)
//     {
//         pcb[0].usems[j] = NULL;
//     }

//     // week7
//     pcb[0].tgid = 0;
//     pcb[0].group_leader = &pcb[0];
//     pcb[0].thread_num = 1;
//     pcb[0].thread_group = NULL;

//     // week7 join&detach
//     pcb[0].joinable = 1;
//     pcb[0].detached = 0;
//     sem_init(&pcb[0].join_sem, 0);

//     // week8 signal
//     pcb[0].sigblocked = 0;
//     list_init(&pcb[0].sigpending_queue);
//     for (int i = 0; i < SIGNAL_NUM; ++i)
//     {
//         pcb[0].sigaction[i] = handle_signal;
//     }
//     // Lab2-1, set status and pgdir
//     // Lab2-4, init zombie_sem
//     // Lab3-2, set cwd
//     pcb[0].cwd = iopen("/", TYPE_NONE);
// }

// proc_t *proc_alloc()
// {
//     // WEEK1: alloc a new proc, find a unused pcb from pcb[1..PROC_NUM-1], return NULL if no such one
//     for (int i = 1; i < PROC_NUM; i++)
//     {
//         if (pcb[i].status == UNUSED)
//         {
//             pcb[i].pid = next_pid;
//             pcb[i].brk = 0;
//             next_pid = next_pid % 32767 + 1;
//             pcb[i].status = UNINIT;
//             pcb[i].pgdir = vm_alloc();
//             pcb[i].kstack = (kstack_t *)kalloc();
//             pcb[i].ctx = &pcb[i].kstack->ctx;
//             pcb[i].parent = NULL;
//             pcb[i].child_num = 0;
//             sem_init(&pcb[i].zombie_sem, 0);
//             for (int j = 0; j < MAX_USEM; j++)
//             {
//                 pcb[i].usems[j] = NULL;
//             }
//             pcb[i].tgid = pcb[i].pid;
//             pcb[i].thread_num = 1;
//             pcb[i].group_leader = &pcb[i];
//             pcb[i].thread_group = NULL;
//             pcb[i].joinable = 1;
//             pcb[i].detached = 0;
//             sem_init(&pcb[i].join_sem, 0);

//             pcb[i].sigblocked = 0;
//             list_init(&pcb[i].sigpending_queue);
//             for (int j = 0; j < SIGNAL_NUM; ++j)
//             {
//                 pcb[i].sigaction[j] = handle_signal;
//             }

//             // 初始化文件描述符号表
//             for (int j = 0; j < MAX_UFILE; ++j)
//             {
//                 pcb[i].files[j] = NULL;
//             }

//             // 初始化当前工作目录
//             pcb[i].cwd = NULL;
//             return &pcb[i];
//         }
//     }
//     return NULL;
// }

// void proc_free(proc_t *proc)
// {
//     // WEEK3-virtual-memory: free proc's pgdir and kstack and mark it UNUSED
//     proc->status = UNUSED;
//     vm_teardown(proc->pgdir);
//     proc->pgdir = NULL;
//     proc->kstack = NULL;
//     proc->ctx = NULL;
//     proc->pid = -1;
//     proc->brk = 0;
//     list_empty(&proc->sigpending_queue);
//     proc->sigblocked = 0;
// }

// void thread_free(proc_t *thread)
// {
//     thread->status = UNUSED;
//     thread->kstack = NULL;
//     thread->ctx = NULL;
//     thread->pid = -1;
//     thread->tgid = -1;
//     thread->group_leader = NULL;
//     thread->thread_group = NULL;
//     list_empty(&thread->sigpending_queue);
// }

// int thread_detached(int tid)
// {
//     proc_t *detach_thread = pid2proc(tid);
//     if (!detach_thread)
//         return -1;
//     detach_thread->joinable = 0;
//     detach_thread->detached = 1;
//     return 0;
// }

// void proc_set_kernel_parent(proc_t *proc)
// {
//     proc->parent = &pcb[0];
//     ++pcb[0].child_num;
// }

// proc_t *proc_curr()
// {
//     return curr;
// }

// void proc_run(proc_t *proc)
// {
//     // WEEK1: start os
//     proc->status = RUNNING;
//     curr = proc;
//     set_cr3(proc->pgdir);
//     set_tss(KSEL(SEG_KDATA), (uint32_t)STACK_TOP(proc->kstack));
//     do_signal(proc);
//     irq_iret(proc->ctx);
// }

// void proc_addready(proc_t *proc)
// {
//     // WEEK4-process-api: mark proc READY
//     proc->status = READY;
// }

// void proc_yield()
// {
//     // WEEK4-process-api: mark curr proc READY, then int $0x81
//     curr->status = READY;
//     INT(0x81);
// }

// void proc_copycurr(proc_t *proc)
// {
//     // WEEK4-process-api: copy curr proc
//     vm_copycurr(proc->pgdir);
//     proc->brk = curr->group_leader->brk;
//     *proc->kstack = *curr->kstack;
//     proc->ctx->eax = 0;
//     curr->child_num++;
//     proc->parent = curr;
//     // WEEK5-semaphore: dup opened usems
//     for (int i = 0; i < MAX_USEM; i++)
//     {
//         proc->usems[i] = curr->group_leader->usems[i];
//         if (proc->usems[i] != NULL)
//             usem_dup(curr->group_leader->usems[i]);
//     }

//     // copy files
//     proc_t *proc_cur = proc_curr()->group_leader;
//     for (int i = 0; i < MAX_UFILE; ++i)
//     {
//         proc->files[i] = proc_cur->files[i];
//     }
//     // Lab3-1: dup opened files
//     // Lab3-2: dup cwd
//     // proc->group_leader->cwd = curr->group_leader->cwd;
//     // if (proc->group_leader->cwd != NULL)
//     // {
//     //     idup(proc->group_leader->cwd);
//     // }

//     proc->group_leader->cwd = curr->group_leader->cwd;
//     if (proc->group_leader->cwd != NULL)
//     {
//         idup(proc->group_leader->cwd);
//     }
// }

// void proc_makezombie(proc_t *proc, int exitcode)
// {
//     // WEEK4-process-api: mark proc ZOMBIE and record exitcode, set children's parent to NULL
//     proc->status = ZOMBIE;
//     proc->exit_code = exitcode;
//     for (int i = 0; i < PROC_NUM; i++)
//     {
//         if (pcb[i].parent == proc)
//             proc_set_kernel_parent(&pcb[i]);
//     }

//     // WEEK5-semaphore: release parent's semaphore
//     if (proc->parent != NULL)
//         sem_v(&proc->parent->zombie_sem);
//     for (int i = 0; i < MAX_USEM; i++)
//     {
//         if (proc->usems[i] != NULL)
//         {
//             usem_close(proc->usems[i]);
//             proc->usems[i] = NULL;
//         }
//     }

//     sem_v(&proc->join_sem);

//     // Lab3-1: close opened files
//     for (int i = 0; i < MAX_UFILE; ++i)
//     {
//         if (proc->group_leader->files[i] != NULL && proc->group_leader->files[i]->inode != NULL)
//         {
//             iclose(proc->files[i]->inode);
//         }
//     }
//     // Lab3-2: close cwd
//     if (proc->cwd != NULL)
//     {
//         iclose(proc->cwd);
//     }
//     // TODO();
// }

// proc_t *proc_findzombie(proc_t *proc)
// {
//     // WEEK4-process-api: find a ZOMBIE whose parent is proc, return NULL if none
//     for (int i = 0; i < PROC_NUM; i++)
//     {
//         if (pcb[i].status == ZOMBIE && pcb[i].parent == proc)
//             return &pcb[i];
//     }
//     return NULL;
// }

// void proc_block()
// {
//     // WEEK4-process-api: mark curr proc BLOCKED, then int $0x81
//     curr->status = BLOCKED;
//     INT(0x81);
// }

// int proc_allocusem(proc_t *proc)
// {
//     // WEEK5: find a free slot in proc->usems, return its index, or -1 if none
//     // TODO();
//     for (int i = 0; i < MAX_USEM; i++)
//     {
//         if (proc->group_leader->usems[i] == NULL)
//             return i;
//     }
//     return -1;
// }

// usem_t *proc_getusem(proc_t *proc, int sem_id)
// {
//     // WEEK5: return proc->usems[sem_id], or NULL if sem_id out of bound
//     // TODO();
//     if (sem_id >= MAX_USEM)
//         return NULL;
//     usem_t *temp_usem = proc->group_leader->usems[sem_id];
//     return temp_usem;
// }

// int proc_allocfile(proc_t *proc)
// {
//     // Lab3-1: find a free slot in proc->files, return its index, or -1 if none
//     for (int i = 0; i < MAX_UFILE; ++i)
//     {
//         if (proc->files[i] == NULL)
//         {
//             return i;
//         }
//     }
//     return -1;
// }

// file_t *proc_getfile(proc_t *proc, int fd)
// {
//     // Lab3-1: return proc->files[fd], or NULL if fd out of bound
//     if (fd >= MAX_UFILE || fd < 0)
//     {
//         return NULL;
//     }
//     return proc->files[fd];
// }

// void schedule(Context *ctx)
// {
//     // WEEK4-process-api: save ctx to curr->ctx, then find a READY proc and run it
//     curr->ctx = ctx;
//     proc_t *next;
//     for (int i = (curr - pcb + 1) % PROC_NUM;; i = (i + 1) % PROC_NUM)
//     {
//         if (pcb[i].status == READY)
//         {
//             // printf("ready_proc = %d\n",pcb[i].pid);
//             next = &pcb[i];
//             break;
//         }
//     }

//     assert(next);
//     proc_run(next);
// }

// // 信号处理函数
// void do_signal(proc_t *proc)
// {
//     list_t *head = &proc->sigpending_queue;
//     if (head == NULL)
//     {
//         printf("No signal to be handled\n");
//         return;
//     }
//     list_t *ptr = head->next;
//     //     if (proc->pid == 2){
//     // // printf("head-addr = %p\n",head);

//     //     }
//     int len = 0;
//     while (ptr != head)
//     {
//         len++;
//         // printf("len= %d\n",len);
//         // printf("enter ");
//         int signo = *((int *)ptr);
//         // printf("signo = %d\n",signo);
//         // printf("proc_id = %d\n",proc->pid);
//         if (proc->sigblocked & (1 << signo))
//         {
//             ptr = ptr->next;
//             continue;
//         }
//         proc->sigaction[signo](signo, proc);
//         list_remove(&proc->sigpending_queue, ptr);
//         break;
//     }
// }

// void handle_signal(int signo, proc_t *proc)
// {
//     // WEEK8-signal
//     assert(signo >= 0 && signo < SIGNAL_NUM);
//     switch (signo)
//     {
//     case SIGSTOP:
//         // Handle SIGHUP logic
//         proc->status = BLOCKED;
//         if (proc == proc_curr())
//         {
//             INT(0x81);
//         }
//         break;

//     case SIGCONT:
//         // TODO: Implement SIGCONT logic here
//         proc->status = READY;
//         break;

//     case SIGKILL:
//         // Handle SIGKILL signal
//         kill_process(proc);
//         break;

//     case SIGUSR1:
//         printf("Signal SIGUSR1 in proc %d is not defined.\n", proc_curr()->tgid);
//         break;

//     case SIGUSR2:
//         printf("Signal SIGUSR2 in proc %d is not defined.\n", proc_curr()->tgid);
//         break;

//     default:
//         printf("Received an invalid signal number: %d\n", signo);
//         panic("Signal error");
//         break;
//     }
// }

// void kill_process(proc_t *kill_proc)
// {
//     if (kill_proc == NULL)
//     {
//         return;
//     }
//     bool status = kill_proc->status == RUNNING;

//     while (kill_proc->thread_num > 1)
//     { // cv-like operation
//         proc_yield();
//     }

//     proc_t *exit_proc = kill_proc->thread_group;
//     while (exit_proc != NULL)
//     {
//         proc_t *temp = exit_proc->thread_group;
//         thread_free(exit_proc);
//         exit_proc = temp;
//     }

//     proc_makezombie(kill_proc, 9);

//     if (status)
//         INT(0x81);
// }

#include "klib.h"
#include "cte.h"
#include "proc.h"

#define PROC_NUM 64

static __attribute__((used)) int next_pid = 1;

proc_t pcb[PROC_NUM];
static proc_t *curr = &pcb[0];

proc_t *pid2proc(int pid)
{
    for (int i = 0; i < PROC_NUM; ++i)
    {
        if (pcb[i].pid == pid)
            return &pcb[i];
    }

    return NULL;
}

void init_proc()
{
    // WEEK1: init proc status
    pcb[0].status = RUNNING;
    pcb[0].pid = 0;
    // WEEK2: add ctx and kstack for interruption
    pcb[0].kstack = (void *)(KER_MEM - PGSIZE);
    pcb[0].ctx = &pcb[0].kstack->ctx;
    // WEEK3: add pgdir
    pcb[0].pgdir = vm_curr();
    pcb[0].parent = NULL;
    pcb[0].child_num = 0;
    // WEEK5: semaphore
    sem_init(&pcb[0].zombie_sem, 0);
    for (int j = 0; j < MAX_USEM; j++)
    {
        pcb[0].usems[j] = NULL;
    }

    // week7
    pcb[0].tgid = 0;
    pcb[0].group_leader = &pcb[0];
    pcb[0].thread_num = 1;
    pcb[0].thread_group = NULL;

    // week7 join&detach
    pcb[0].joinable = 1;
    pcb[0].detached = 0;
    sem_init(&pcb[0].join_sem, 0);

    // week8 signal
    pcb[0].sigblocked = 0;
    list_init(&pcb[0].sigpending_queue);
    pcb[0].sigpending_queue.ptr = (void *)SIGNAL_NUM;
    for (int j = 0; j < SIGNAL_NUM; ++j)
    {
        pcb[0].sigaction[j] = handle_signal;
    }

    // week9
    for (int j = 0; j < MAX_UFILE; ++j)
        pcb[0].files[j] = NULL;

    // Lab2-1, set status and pgdir
    // Lab2-4, init zombie_sem
    // Lab3-2, set cwd

    pcb[0].cwd = iopen("/", TYPE_NONE);
}

proc_t *proc_alloc()
{
    // WEEK1: alloc a new proc, find a unused pcb from pcb[1..PROC_NUM-1], return NULL if no such one
    for (int i = 1; i < PROC_NUM; i++)
    {
        if (pcb[i].status == UNUSED)
        {
            pcb[i].pid = next_pid;
            pcb[i].brk = 0;
            next_pid = next_pid % 32767 + 1;
            pcb[i].status = UNINIT;
            pcb[i].pgdir = vm_alloc();
            pcb[i].kstack = (kstack_t *)kalloc();
            pcb[i].ctx = &pcb[i].kstack->ctx;
            pcb[i].parent = NULL;
            pcb[i].child_num = 0;

            sem_init(&pcb[i].zombie_sem, 0);
            for (int j = 0; j < MAX_USEM; j++)
                pcb[i].usems[j] = NULL;

            pcb[i].tgid = pcb[i].pid;
            pcb[i].thread_num = 1;
            pcb[i].group_leader = &pcb[i];
            pcb[i].thread_group = NULL;
            pcb[i].joinable = 1;
            pcb[i].detached = 0;
            sem_init(&pcb[i].join_sem, 0);

            pcb[i].sigblocked = 0;
            list_init(&pcb[i].sigpending_queue);
            pcb[i].sigpending_queue.ptr = (void *)SIGNAL_NUM;
            for (int j = 0; j < SIGNAL_NUM; ++j)
                pcb[i].sigaction[j] = handle_signal;

            for (int j = 0; j < MAX_UFILE; ++j)
                pcb[i].files[j] = NULL;

            // week10
            pcb[i].cwd = NULL;

            return &pcb[i];
        }
    }
    return NULL;
}

void proc_free(proc_t *proc)
{
    // WEEK3-virtual-memory: free proc's pgdir and kstack and mark it UNUSED
    proc->status = UNUSED;
    vm_teardown(proc->pgdir);
    proc->pgdir = NULL;
    proc->kstack = NULL;
    proc->ctx = NULL;
    proc->pid = -1;
    proc->brk = 0;

    proc->sigblocked = 0;
    list_empty(&proc->sigpending_queue);
}

void thread_free(proc_t *thread)
{
    thread->status = UNUSED;
    thread->kstack = NULL;
    thread->ctx = NULL;
    thread->pid = -1;
    thread->tgid = -1;
    thread->group_leader = NULL;
    thread->thread_group = NULL;

    thread->sigblocked = 0;
    list_empty(&thread->sigpending_queue);
}

int thread_detached(int tid)
{
    proc_t *detach_thread = pid2proc(tid);
    if (!detach_thread)
        return -1;
    detach_thread->joinable = 0;
    detach_thread->detached = 1;
    return 0;
}

void proc_set_kernel_parent(proc_t *proc)
{
    proc->parent = &pcb[0];
    ++pcb[0].child_num;
}

proc_t *proc_curr()
{
    return curr;
}

void proc_run(proc_t *proc)
{
    // WEEK1: start os
    proc->status = RUNNING;
    curr = proc;
    set_cr3(proc->pgdir);
    set_tss(KSEL(SEG_KDATA), (uint32_t)STACK_TOP(proc->kstack));
    do_signal(proc);
    irq_iret(proc->ctx);
}

void proc_addready(proc_t *proc)
{
    // WEEK4-process-api: mark proc READY
    proc->status = READY;
}

void proc_yield()
{
    // WEEK4-process-api: mark curr proc READY, then int $0x81
    curr->status = READY;
    INT(0x81);
}

void proc_copycurr(proc_t *proc)
{
    // WEEK4-process-api: copy curr proc
    vm_copycurr(proc->pgdir);
    proc->brk = curr->group_leader->brk;
    *proc->kstack = *curr->kstack;
    proc->ctx->eax = 0;
    curr->child_num++;
    proc->parent = curr;
    // WEEK5-semaphore: dup opened usems
    for (int i = 0; i < MAX_USEM; i++)
    {
        proc->usems[i] = curr->group_leader->usems[i];
        if (proc->usems[i] != NULL)
            usem_dup(curr->group_leader->usems[i]);
    }

    // Week9
    // Lab3-1: dup opened files
    for (int i = 0; i < MAX_UFILE; ++i)
    {
        proc->files[i] = curr->group_leader->files[i];
        // printf("to pid = %d files[%d] = %p\n",proc->pid,i,proc->group_leader->files[i]);
        if (proc->files[i] != NULL)
        {
            fdup(curr->group_leader->files[i]);
            printf("files[%d].ref = %d\n", i, proc->files[i]->ref);
        }
    }

    // Lab3-2: dup cwd
    proc->group_leader->cwd = curr->group_leader->cwd;
    if (proc->group_leader->cwd != NULL)
        idup(proc->group_leader->cwd);
}

void proc_makezombie(proc_t *proc, int exitcode)
{
    // WEEK4-process-api: mark proc ZOMBIE and record exitcode, set children's parent to NULL
    proc->status = ZOMBIE;
    proc->exit_code = exitcode;
    for (int i = 0; i < PROC_NUM; i++)
    {
        if (pcb[i].parent == proc)
            proc_set_kernel_parent(&pcb[i]);
    }

    // WEEK5-semaphore: release parent's semaphore
    if (proc->parent != NULL)
        sem_v(&proc->parent->zombie_sem);
    for (int i = 0; i < MAX_USEM; i++)
    {
        if (proc->usems[i] != NULL)
        {
            usem_close(proc->usems[i]);
            proc->usems[i] = NULL;
        }
    }

    sem_v(&proc->join_sem);

    // Week9
    // Lab3-1: close opened files
    for (int i = 0; i < MAX_UFILE; ++i)
        if (proc->files[i] && proc->files[i]->type == TYPE_FILE && proc->files[i]->inode != NULL)
            iclose(proc->files[i]->inode);

    // Lab3-2: close cwd
    // TODO();
    if (proc->cwd != NULL)
        iclose(proc->cwd);
}

proc_t *proc_findzombie(proc_t *proc)
{
    // WEEK4-process-api: find a ZOMBIE whose parent is proc, return NULL if none
    for (int i = 0; i < PROC_NUM; i++)
    {
        if (pcb[i].status == ZOMBIE && pcb[i].parent == proc)
            return &pcb[i];
    }
    return NULL;
}

void proc_block()
{
    // WEEK4-process-api: mark curr proc BLOCKED, then int $0x81
    curr->status = BLOCKED;
    INT(0x81);
}

int proc_allocusem(proc_t *proc)
{
    // WEEK5: find a free slot in proc->usems, return its index, or -1 if none
    // TODO();
    for (int i = 0; i < MAX_USEM; i++)
    {
        if (proc->group_leader->usems[i] == NULL)
            return i;
    }
    return -1;
}

usem_t *proc_getusem(proc_t *proc, int sem_id)
{
    // WEEK5: return proc->usems[sem_id], or NULL if sem_id out of bound
    // TODO();
    if (sem_id >= MAX_USEM)
        return NULL;
    usem_t *temp_usem = proc->group_leader->usems[sem_id];
    return temp_usem;
}

int proc_allocfile(proc_t *proc)
{
    // Lab3-1: find a free slot in proc->files, return its index, or -1 if none
    // TODO();

    for (int i = 0; i < MAX_UFILE; ++i)
    {
        if (proc->files[i] == NULL)
            return i;
    }

    return -1;
}

file_t *proc_getfile(proc_t *proc, int fd)
{
    // Lab3-1: return proc->files[fd], or NULL if fd out of bound
    // TODO();
    if (fd >= MAX_UFILE)
        return NULL;
    return proc->files[fd];
}

void schedule(Context *ctx)
{
    // WEEK4-process-api: save ctx to curr->ctx, then find a READY proc and run it
    curr->ctx = ctx;
    proc_t *next;
    for (int i = (curr - pcb + 1) % PROC_NUM;; i = (i + 1) % PROC_NUM)
    {
        if (pcb[i].status == READY)
        {
            next = &pcb[i];
            break;
        }
    }

    assert(next);
    proc_run(next);
}

void do_signal(proc_t *proc)
{
    //  遍历signal list 查找阻塞signal
    int sigblocked = proc->sigblocked;
    list_t *next_signal = &proc->sigpending_queue;
    list_t *origin_signal = next_signal;
    while (next_signal != NULL)
    {
        int signo = (int)next_signal->ptr;
        if (signo >= 0 && signo < SIGNAL_NUM)
        {
            if (!(sigblocked & (1 << signo)))
            {
                proc->sigaction[signo](signo, proc);
                list_remove(&proc->sigpending_queue, next_signal);
                break;
            }
        }
        next_signal = next_signal->next;
        if (next_signal == origin_signal)
            break;
    }
}

void handle_signal(int signo, proc_t *proc)
{
    // WEEK8-signal
    assert(signo >= 0 && signo < SIGNAL_NUM);
    switch (signo)
    {
    case SIGSTOP:
        // Handle SIGHUP logic
        // TODO();
        bool is_running = proc->status == RUNNING;
        proc->status = BLOCKED;
        if (is_running)
            INT(0x81);
        break;

    case SIGCONT:
        // TODO: Implement SIGCONT logic here
        // TODO();
        proc->status = READY;
        break;

    case SIGKILL:
        // Handle SIGKILL signal
        // TODO();
        bool isRunning = proc->status == RUNNING;

        while (proc->thread_num > 1)
        { // cv-like operation
            proc_yield();
        }

        proc_t *exit_proc = proc->thread_group;
        while (exit_proc != NULL)
        {
            proc_t *temp = exit_proc->thread_group;
            thread_free(exit_proc);
            exit_proc = temp;
        }

        proc_makezombie(proc, 9);

        if (isRunning)
            INT(0x81);
        break;

    case SIGUSR1:
        printf("Signal SIGUSR1 in proc %d is not defined.\n", proc_curr()->tgid);
        break;

    case SIGUSR2:
        printf("Signal SIGUSR2 in proc %d is not defined.\n", proc_curr()->tgid);
        break;

    default:
        printf("Received an invalid signal number: %d\n", signo);
        panic("Signal error");
        break;
    }
}
