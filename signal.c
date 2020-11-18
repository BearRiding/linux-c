/*
 *  linux/kernel/signal.c
 *
 *  (C) 1991  Linus Torvalds
 * 6、分析初始化IDT、GDT、LDT的代码。
IDT:
初始化各种异常处理服务程序的中断描述符
将IDT的int 0x11~0x2F都初始化，将IDT中对应的指向中断服务程序的指针设置为reserved
设置协处理器的IDT项
允许主8259A中断控制器的IRQ32、IRQ3的中断请求
设置并口（可以接打印机）的IDT项
```
//init IDT
void trap_init(void)
{
    int i;

    set_trap_gate(0,&divide_error);
    set_trap_gate(1,&debug);
    set_trap_gate(2,&nmi);
    set_system_gate(3,&int3);    /* int3-5 can be called from all */
    set_system_gate(4,&overflow);
    set_system_gate(5,&bounds);
    set_trap_gate(6,&invalid_op);
    set_trap_gate(7,&device_not_available);
    set_trap_gate(8,&double_fault);
    set_trap_gate(9,&coprocessor_segment_overrun);
    set_trap_gate(10,&invalid_TSS);
    set_trap_gate(11,&segment_not_present);
    set_trap_gate(12,&stack_segment);
    set_trap_gate(13,&general_protection);
    set_trap_gate(14,&page_fault);
    set_trap_gate(15,&reserved);
    set_trap_gate(16,&coprocessor_error);
    for (i=17;i<48;i++)
        set_trap_gate(i,&reserved);
    set_trap_gate(45,&irq13);
    outb_p(inb_p(0x21)&0xfb,0x21);
    outb(inb_p(0xA1)&0xdf,0xA1);
    set_trap_gate(39,&parallel_interrupt);
```
在GDT中初始化进程0所占的4、5两项，即初始化TSS0和LDT0。具体来说，就是这两行代码在GDT表中的第5项和第6项的地方挖了两个空间分别放TSS0和LDT0。这步是初始化进程0相关的管理结构的最后一步：将TR寄存器指向TSS0、LDTR寄存器指向LDT0，这样CPU就可以通过这两个寄存器找到TSS0和LDT0，也就能找到一切和进程0相关的管理信息。
set_tss_desc(gdt+FIRST_TSS_ENTRY,&(init_task.task.tss));
set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));

7、在sched_init(void)函数中有这样的代码：
for(i=1;i<NR_TASKS;i++) {
    task[i] = NULL;
    ……
但并未涉及task[0]，从后续代码能感觉到已经给了进程0，请给出代码证据。？
union task_union{
    struct task_struct task;
    char   stack[PAGE_SIZE];
}
static union task_union init_task = {INIT_TASK,};

struct task_struct *task[NR_TASKS] = {&(init_task.task),};
在初始化进程槽 task[NR_TASKS]时将第一项task[0]分配给了进程0占用


 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <signal.h>

volatile void do_exit(int error_code);

int sys_sgetmask()
{
	return current->blocked;
}

int sys_ssetmask(int newmask)
{
	int old=current->blocked;

	current->blocked = newmask & ~(1<<(SIGKILL-1));
	return old;
}

static inline void save_old(char * from,char * to)
{
	int i;

	verify_area(to, sizeof(struct sigaction));
	for (i=0 ; i< sizeof(struct sigaction) ; i++) {
		put_fs_byte(*from,to);
		from++;
		to++;
	}
}

static inline void get_new(char * from,char * to)
{
	int i;

	for (i=0 ; i< sizeof(struct sigaction) ; i++)
		*(to++) = get_fs_byte(from++);
}

int sys_signal(int signum, long handler, long restorer)
{
	struct sigaction tmp;

	if (signum<1 || signum>32 || signum==SIGKILL)
		return -1;
	tmp.sa_handler = (void (*)(int)) handler;
	tmp.sa_mask = 0;
	tmp.sa_flags = SA_ONESHOT | SA_NOMASK;
	tmp.sa_restorer = (void (*)(void)) restorer;
	handler = (long) current->sigaction[signum-1].sa_handler;
	current->sigaction[signum-1] = tmp;
	return handler;
}

int sys_sigaction(int signum, const struct sigaction * action,
	struct sigaction * oldaction)
{
	struct sigaction tmp;

	if (signum<1 || signum>32 || signum==SIGKILL)
		return -1;
	tmp = current->sigaction[signum-1];
	get_new((char *) action,
		(char *) (signum-1+current->sigaction));
	if (oldaction)
		save_old((char *) &tmp,(char *) oldaction);
	if (current->sigaction[signum-1].sa_flags & SA_NOMASK)
		current->sigaction[signum-1].sa_mask = 0;
	else
		current->sigaction[signum-1].sa_mask |= (1<<(signum-1));
	return 0;
}

void do_signal(long signr,long eax, long ebx, long ecx, long edx,
	long fs, long es, long ds,
	long eip, long cs, long eflags,
	unsigned long * esp, long ss)
{
	unsigned long sa_handler;
	long old_eip=eip;
	struct sigaction * sa = current->sigaction + signr - 1;
	int longs;
	unsigned long * tmp_esp;

	sa_handler = (unsigned long) sa->sa_handler;
	if (sa_handler==1)
		return;
	if (!sa_handler) {
		if (signr==SIGCHLD)
			return;
		else
			do_exit(1<<(signr-1));
	}
	if (sa->sa_flags & SA_ONESHOT)
		sa->sa_handler = NULL;
	*(&eip) = sa_handler;
	longs = (sa->sa_flags & SA_NOMASK)?7:8;
	*(&esp) -= longs;
	verify_area(esp,longs*4);
	tmp_esp=esp;
	put_fs_long((long) sa->sa_restorer,tmp_esp++);
	put_fs_long(signr,tmp_esp++);
	if (!(sa->sa_flags & SA_NOMASK))
		put_fs_long(current->blocked,tmp_esp++);
	put_fs_long(eax,tmp_esp++);
	put_fs_long(ecx,tmp_esp++);
	put_fs_long(edx,tmp_esp++);
	put_fs_long(eflags,tmp_esp++);
	put_fs_long(old_eip,tmp_esp++);
	current->blocked |= sa->sa_mask;
}
