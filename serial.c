/*
 *  linux/kernel/serial.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *	serial.c
 *
 * This module implements the rs232 io functions
 *	void rs_write(struct tty_struct * queue);
 *	void rs_init(void);
 * and all interrupts pertaining to serial IO.
 * 
 * 30、make_request（）函数    
    if (req < request) {
        if (rw_ahead) {
            unlock_buffer(bh);
            return;
        }
        sleep_on(&wait_for_request);
        goto repeat;
其中的sleep_on(&wait_for_request)是谁在等？等什么？

这行代码是当前进程在等（如：进程1），在等空闲请求项。
make_request()函数创建请求项并插入请求队列，执行if的内容说明没有找到空请求项：如果是超前的读写请求，因为是特殊情况则放弃请求直接释放缓冲区，否则是一般的读写操作，此时等待直到有空闲请求项，然后从repeat开始重新查看是否有空闲的请求项。


---------------------------------------------------------

5、用户进程自己设计一套 LDT 表，并与 GDT 挂接，是否可行，为什么？（P259）
不可行。首先，用户进程不可以设置 GDT、LDT，因为 Linux0.11 将 GDT、LDT 这两个数 据结构设置在内核数据区，是 0 特权级的，只有 0 特权级的额代码才能修改设置 GDT、LDT； 而且，用户也不可以在自己的数据段按照自己的意愿重新做一套 GDT、LDT，如果仅仅是 形式上做一套和 GDT、LDT 一样的数据结构是可以的，但是真正起作用的 GDT、LDT 是 CPU 硬件认定的，这两个数据结构的首地址必须挂载在 CPU 中的 GDTR、LDTR 上，运行 时 CPU 只认 GDTR 和 LDTR 指向的数据结构，其他数据结构就算起名字叫 GDT、LDT， CPU 也一概不认；另外，用户进程也不能将自己制作的 GDT、LDT 挂接到 GDRT、LDRT 上，因为对 GDTR 和 LDTR 的设置只能在 0 特权级别下执行,3 特权级别下无法把这套结构 挂接在 CR3 上。

 */

#include <linux/tty.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/io.h>

#define WAKEUP_CHARS (TTY_BUF_SIZE/4)

extern void rs1_interrupt(void);
extern void rs2_interrupt(void);

static void init(int port)
{
	outb_p(0x80,port+3);	/* set DLAB of line control reg */
	outb_p(0x30,port);	/* LS of divisor (48 -> 2400 bps */
	outb_p(0x00,port+1);	/* MS of divisor */
	outb_p(0x03,port+3);	/* reset DLAB */
	outb_p(0x0b,port+4);	/* set DTR,RTS, OUT_2 */
	outb_p(0x0d,port+1);	/* enable all intrs but writes */
	(void)inb(port);	/* read data port to reset things (?) */
}

void rs_init(void)
{
	set_intr_gate(0x24,rs1_interrupt);
	set_intr_gate(0x23,rs2_interrupt);
	init(tty_table[1].read_q.data);
	init(tty_table[2].read_q.data);
	outb(inb_p(0x21)&0xE7,0x21);
}

/*
 * This routine gets called when tty_write has put something into
 * the write_queue. It must check wheter the queue is empty, and
 * set the interrupt register accordingly
 *
 *	void _rs_write(struct tty_struct * tty);
 */
void rs_write(struct tty_struct * tty)
{
	cli();
	if (!EMPTY(tty->write_q))
		outb(inb_p(tty->write_q.data+1)|0x02,tty->write_q.data+1);
	sti();
}
