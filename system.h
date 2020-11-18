/**
 * 18、内核和普通用户进程并不在一个线性地址空间内，为什么仍然能够访问普通用户进程的页面？（P272）
内核的线性地址空间和用户进程不一样，内核是不能通过跨越线性地址访问进程的，但由于早就占有了所有的页面，而且特权级是 0，所以内核执行时，可以对所有的内容进行改动，“等价于”可以操作所有进程所在的页面。


19、 copy_mem（）和 copy_page_tables（）在第一次调用时是如何运行的？
copy_mem()的第一次调用是进程 0 创建进程 1 时，它先提取当前进程（进程 0）的代码段、数据段的段限长，并将当前进程（进程 0）的段限长赋值给子进程（进程 1）的段限长。然后提取当前进程（进程 0）的代码段、数据段的段基址，检查当前进程（进程 0）的段基址、段限长是否有问题。接着设置子进程（进程 1）的 LDT 段描述符中代码段和数据段的基地址为 nr(1)*64MB。最后调用 copy_page_table()函数
copy_page_table()的参数是源地址、目的地址和大小，首先检测源地址和目的地址是否都是 4MB 的整数倍，如不是则报错， 不符合分页要求。然后取源地址和目的地址所对应的页目录项地址，检测如目的地址所对应的页目录表项已被使用则报错，其中源地址不一定是连续使用的，所以有不存在的跳过。接着，取源地址的页表地址，并为目的地址申请一个新页作为子进程的页表，且修改为已使用。然后，判断是否源地址为 0，即父进程是否为进程 0 ，如果是，则复制页表项数为 160，否则为 1k。最后将源页表项复制给目的页表，其中将目的页表项内的页设为“只读”，源页表项内的页地址超过 1M 的部分也设为"只读"（由于是第一次调用，所以父进程是 0，都在 1M 内，所以都不设为“只读”），并在 mem_map中所对应的项引用计数加 1。 1M 内的内核区不参与用户分页管理。

 * 
*/


#define move_to_user_mode() \ // 模仿中断硬件压栈，顺序是ss esp eflags， cs， eip
__asm__ ("movl %%esp,%%eax\n\t" \ 
	"pushl $0x17\n\t" \ // ss进栈
	"pushl %%eax\n\t" \
	"pushfl\n\t" \
	"pushl $0x0f\n\t" \
	"pushl $1f\n\t" \
	"iret\n" \  // 出栈恢复现场，翻转特权级从0到3
	"1:\tmovl $0x17,%%eax\n\t" \
	"movw %%ax,%%ds\n\t" \
	"movw %%ax,%%es\n\t" \
	"movw %%ax,%%fs\n\t" \
	"movw %%ax,%%gs" \
	:::"ax")

#define sti() __asm__ ("sti"::)
#define cli() __asm__ ("cli"::)
#define nop() __asm__ ("nop"::)

#define iret() __asm__ ("iret"::)

#define _set_gate(gate_addr,type,dpl,addr) \
__asm__ ("movw %%dx,%%ax\n\t" \
	"movw %0,%%dx\n\t" \
	"movl %%eax,%1\n\t" \
	"movl %%edx,%2" \
	: \
	: "i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
	"o" (*((char *) (gate_addr))), \
	"o" (*(4+(char *) (gate_addr))), \
	"d" ((char *) (addr)),"a" (0x00080000))

#define set_intr_gate(n,addr) \
	_set_gate(&idt[n],14,0,addr)

#define set_trap_gate(n,addr) \
	_set_gate(&idt[n],15,0,addr)

#define set_system_gate(n,addr) \
	_set_gate(&idt[n],15,3,addr)

#define _set_seg_desc(gate_addr,type,dpl,base,limit) {\
	*(gate_addr) = ((base) & 0xff000000) | \
		(((base) & 0x00ff0000)>>16) | \
		((limit) & 0xf0000) | \
		((dpl)<<13) | \
		(0x00408000) | \
		((type)<<8); \
	*((gate_addr)+1) = (((base) & 0x0000ffff)<<16) | \
		((limit) & 0x0ffff); }

#define _set_tssldt_desc(n,addr,type) \
__asm__ ("movw $104,%1\n\t" \
	"movw %%ax,%2\n\t" \
	"rorl $16,%%eax\n\t" \
	"movb %%al,%3\n\t" \
	"movb $" type ",%4\n\t" \
	"movb $0x00,%5\n\t" \
	"movb %%ah,%6\n\t" \
	"rorl $16,%%eax" \
	::"a" (addr), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
	)

#define set_tss_desc(n,addr) _set_tssldt_desc(((char *) (n)),addr,"0x89")
#define set_ldt_desc(n,addr) _set_tssldt_desc(((char *) (n)),addr,"0x82")
