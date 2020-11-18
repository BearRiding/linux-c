/*
 * linux/kernel/math/math_emulate.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * This directory should contain the math-emulation code.
 * Currently only results in a signal.
 * 
 * 5进程 0 的 task_struct 在哪？具体内容是什么？给出代码证据。
进程 0 的 task_struct 位于内核数据区，即 task 结构的第 0 项 init_task。struct task_struct * task[NR_TASKS] = {&(init_task.task), };
具体内容：包含了进程 0 的进程状态、进程 0 的 LDT、进程 0 的 TSS 等等。其中 ldt 设置了代码段和堆栈段的基址和限长(640KB)，而 TSS 则保存了各种寄存器的值，包括各个段选择符。具体值如下：（课本 P68）

7.进程 0 fork 进程 1 之前，为什么先要调用 move_to_user_mode()？用的是什么方法？解释 其中的道理。（P78-79）
因为在 Linux-0.11 中，除进程 0 之外，所有进程都是由一个已有进程在用户态下完成创建的。 但是此时进程 0 还处于内核态，因此要调用 move_to_user_mode()函数，模仿中断返回的方 式，实现进程 0 的特权级从内核态转化为用户态。又因为在 Linux-0.11 中，转换特权级时采 用中断和中断返回的方式，调用系统中断实现从 3 到 0 的特权级转换，中断返回时转换为 3 特权级。因此，进程 0 从 0 特权级到 3 特权级转换时采用的是模仿中断返回。
设计者通过代码模拟 int（中断） 压栈， 当执行 iret 指令时， CPU 将SS,ESP,EFLAGS,CS,EIP 5 个寄存器的值按序恢复给 CPU， CPU之后翻转到 3 特权级去执行代码。

 */

#include <signal.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

void math_emulate(long edi, long esi, long ebp, long sys_call_ret,
	long eax,long ebx,long ecx,long edx,
	unsigned short fs,unsigned short es,unsigned short ds,
	unsigned long eip,unsigned short cs,unsigned long eflags,
	unsigned short ss, unsigned long esp)
{
	unsigned char first, second;

/* 0x0007 means user code space */
	if (cs != 0x000F) {
		printk("math_emulate: %04x:%08x\n\r",cs,eip);
		panic("Math emulation needed in kernel");
	}
	first = get_fs_byte((char *)((*&eip)++));
	second = get_fs_byte((char *)((*&eip)++));
	printk("%04x:%08x %02x %02x\n\r",cs,eip-2,first,second);
	current->signal |= 1<<(SIGFPE-1);
}

void math_error(void)
{
	__asm__("fnclex");
	if (last_task_used_math)
		last_task_used_math->signal |= 1<<(SIGFPE-1);
}
