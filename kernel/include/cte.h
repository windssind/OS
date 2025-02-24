#ifndef __CTE_H__
#define __CTE_H__

#include <stdint.h>

// TODO: WEEK2: adjust the struct to the correct order
// TODO: WEEK2: add esp and ss
typedef struct Context
{
  uint32_t ds, ebp, edi, esi, edx, ecx, ebx, eax, irq,
      errcode, eip, cs, eflags,esp,ss;
} Context;

void init_cte();
void irq_iret(Context *ctx) __attribute__((noreturn)); // 通过这个进行新程序的运行

void do_syscall(Context *ctx);
void exception_debug_handler(Context *ctx);

#endif
