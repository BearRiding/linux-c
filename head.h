#ifndef _HEAD_H
#define _HEAD_H

/** 
 * 10.为什么不用 call，而是用 ret“调用”main 函数？画出调用路线图，给出代码证据。（图在 P42）
 * call 指令会将 EIP 的值自动压栈，保护返回现场，然后执行被调函数的程序，
 * 等到执行被调 函数的 ret 指令时，自动出栈给 EIP 并还原现场，继续执行 call 的下一条指令。
 * 然而对操作 系统的 main 函数来说，如果用 call 调用 main 函数，
 * 那么 ret 时返回给谁呢？因为没有更底 层的函数程序接收操作系统的返回。
 * 用 ret 实现的调用操作当然就不需要返回了，call 做的 压栈和跳转动作需要手工编写代码。
		after_page_tables:
		pushl $__main; //将 main 的地址压入栈，即 EIP
		setup_paging:
		ret; //弹出 EIP，针对 EIP 指向的值继续执行，即 main 函数的入口地址。

 * 
*/

typedef struct desc_struct {
	unsigned long a,b;
} desc_table[256];

extern unsigned long pg_dir[1024];
extern desc_table idt,gdt;

#define GDT_NUL 0
#define GDT_CODE 1
#define GDT_DATA 2
#define GDT_TMP 3

#define LDT_NUL 0
#define LDT_CODE 1
#define LDT_DATA 2

#endif
