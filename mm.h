#ifndef _MM_H
#define _MM_H

/** 
 * 13.假设：经过一段时间的运行，操作系统中已经有 5 个进程在运行，且内核分别为进程4、进程5 分别创建了第一个页表，这两个页表在谁的线性地址空间？用图表示这两个页表在线性地址空间和物理地址空间的映射关系。
这两个页面均占用内核的线性空间。
 
 14.#define switch_to(n) {\
struct {long a,b;} __tmp; \
__asm__("cmpl %%ecx,_current\n\t" \
         "je 1f\n\t" \
         "movw %%dx,%1\n\t" \
         "xchgl %%ecx,_current\n\t" \
         "ljmp %0\n\t" \
         "cmpl %%ecx,_last_task_used_math\n\t" \
         "jne 1f\n\t" \
         "clts\n" \
         "1:" \
         ::"m" (*&__tmp.a),"m" (*&__tmp.b), \
         "d" (_TSS(n)),"c" ((long) task[n])); \
}
代码中的"ljmp %0\n\t" 很奇怪，按理说jmp指令跳转到得位置应该是一条指令的地址，可是这行代码却跳到了"m" (*&__tmp.a)，这明明是一个数据的地址，更奇怪的，这行代码竟然能正确执行。请论述其中的道理。
答：其中a对应EIP，b对应CS，ljmp此时通过CPU中的电路进行硬件切换，进程由当前进程切换到进程n。CPU将当前寄存器的值保存到当前进程的TSS中，将进程n的TSS数据及LDT的代码段和数据段描述符恢复给CPU的各个寄存器，实现任务切换。

 * 
 * 
 * 
*/

#define PAGE_SIZE 4096

extern unsigned long get_free_page(void);
extern unsigned long put_page(unsigned long page,unsigned long address);
extern void free_page(unsigned long addr);

#endif
