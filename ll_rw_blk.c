/*
 *  linux/kernel/blk_dev/ll_rw.c
 *
 * (C) 1991 Linus Torvalds
 * 
 * 1、进程0的task_struct、内核栈、用户栈在哪？证明进程0的用户栈就是未激活进程0时的0特权栈，
 * 即user_stack，而进程0的内核栈并不是user_stack，给出代码证据。
 * 
进程0的task_struct位于内核数据区，因为在进程0未激活之前，使用的是boot阶段的user_stack，
因此存储在user_stack中。
进程0的task_struct是操作系统设计者事先写好的，
和内核栈一起在task_union内，位于内核数据区，存储在user_stack中。
(因为在进程0未激活之前，使用的是boot阶段的user_stack。) 
在sched_init函数内，进程0的LDT和TSS一开始初始化完毕后就未修改过。
在tss内，ss0指向了段选择子0x10指向的地址，即user_stack。
随后在进程0反转特权级时，ss0被压栈后被弹出给ss，完成了用户栈的建立，仍指向user_stack。因此进程0的用户栈其实就是user_stack。


进程0的task_struct、内核栈、用户栈都在内核数据段。
进程0的task_struct 的具体内容如下代码所示：
// 进程0的task_struct
#define INIT_TASK \
/* state etc */	{ 0,15,15, \
/* signals */	0,{{},},0, \
/* ec,brk... */	0,0,0,0,0,0, \
/* pid etc.. */	0,-1,0,0,0, \
/* uid etc */	0,0,0,0,0,0, \
/* alarm */	0,0,0,0,0,0, \
/* math */	0, \
/* fs info */	-1,0022,NULL,NULL,NULL,0, \
/* filp */	{NULL,}, \
	{ \
		{0,0}, \
/* ldt */	{0x9f,0xc0fa00}, \
		{0x9f,0xc0f200}, \
	}, \
/*tss*/	{0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&pg_dir,\
	 0,0,0,0,0,0,0,0, \
	 0,0,0x17,0x17,0x17,0x17,0x17,0x17, \
	 _LDT(0),0x80000000, \
		{} \
	}, \
}
以下代码说明user_stack在内核数据段。（0x10）一个任务的数据结构与其内核态堆栈是放在同一内存页中。
所以进程0的内核栈是跟着task struct后面的，所以进程0的内核栈不是user_stack，
user_stack是进程0的用户栈。（P91的图）
long user_stack [ PAGE_SIZE>>2 ] ;//定义用户堆栈4K，指针指向最后一项
//该结构用于设置堆栈ss：esp（数据段选择符）
struct {
	long * a;
	short b;
	} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };//内核数据段

//每个进程在内核态运行时都有自己的内核态堆栈，这里定义了内核态堆栈的结构
union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};
static union task_union init_task = {INIT_TASK,};

从 head.s 程序起，系统开始正式在保护模式下运行。此时堆栈段被设置为内核数据段（ 0x10），
堆栈指针 esp 设置成指向 user_stack 数组的顶端，保留了 1 页内存（ 4K）作为堆栈使用。 
user_stack 数组共含有 1024 个长字。此时该堆栈是内核程序自己使用的堆栈。
任务 0 的代码段和数据段相同，段基地址都是从 0 开始，限长也都是 640KB。
这个地址范围也就是内核代码和基本数据所在的地方。在执行了 move_to_user_mode()之后，
它的内核态堆栈位于其任务数据结构所在页面的末端，而它的用户态堆栈就是前面进入保护模式后所使用的堆栈，
也即 sched.c 的user_stack 数组的位置。任务 0 的内核态堆栈是在其人工设置的初始化任务数据结构中指定的，
而它的用户态堆栈是在执行movie_to_user_mode()时，在模拟 iret 返回之前的堆栈中设置的。
在该堆栈中， esp 仍然是 user_stack 中原来的位置，而 ss 被设置成 0x17，
也即用户态局部表中的数据段，也即从内存地址 0开始并且限长为 640KB 的段。

 */



/*
 * This handles all read/write requests to block devices
 */
#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#include "blk.h"

/*
 * The request-struct contains all necessary data
 * to load a nr of sectors into memory
 */
struct request request[NR_REQUEST];

/*
 * used to wait on when there are no free requests
 */
struct task_struct * wait_for_request = NULL;

/* blk_dev_struct is:
 *	do_request-address
 *	next-request
 */
struct blk_dev_struct blk_dev[NR_BLK_DEV] = {
	{ NULL, NULL },		/* no_dev */
	{ NULL, NULL },		/* dev mem */
	{ NULL, NULL },		/* dev fd */
	{ NULL, NULL },		/* dev hd */
	{ NULL, NULL },		/* dev ttyx */
	{ NULL, NULL },		/* dev tty */
	{ NULL, NULL }		/* dev lp */
};

static inline void lock_buffer(struct buffer_head * bh)
{
	cli();
	while (bh->b_lock)
		sleep_on(&bh->b_wait);
	bh->b_lock=1;
	sti();
}

static inline void unlock_buffer(struct buffer_head * bh)
{
	if (!bh->b_lock)
		printk("ll_rw_block.c: buffer not locked\n\r");
	bh->b_lock = 0;
	wake_up(&bh->b_wait);
}

/*
 * add-request adds a request to the linked list.
 * It disables interrupts so that it can muck with the
 * request-lists in peace.
 */
static void add_request(struct blk_dev_struct * dev, struct request * req)
{
	struct request * tmp;

	req->next = NULL;
	cli();
	if (req->bh)
		req->bh->b_dirt = 0;
	if (!(tmp = dev->current_request)) {
		dev->current_request = req;
		sti();
		(dev->request_fn)();
		return;
	}
	for ( ; tmp->next ; tmp=tmp->next)
		if ((IN_ORDER(tmp,req) ||
		    !IN_ORDER(tmp,tmp->next)) &&
		    IN_ORDER(req,tmp->next))
			break;
	req->next=tmp->next;
	tmp->next=req;
	sti();
}

static void make_request(int major,int rw, struct buffer_head * bh)
{
	struct request * req;
	int rw_ahead;

/* WRITEA/READA is special case - it is not really needed, so if the */
/* buffer is locked, we just forget about it, else it's a normal read */
	if (rw_ahead = (rw == READA || rw == WRITEA)) {
		if (bh->b_lock)
			return;
		if (rw == READA)
			rw = READ;
		else
			rw = WRITE;
	}
	if (rw!=READ && rw!=WRITE)
		panic("Bad block dev command, must be R/W/RA/WA");
	lock_buffer(bh);
	if ((rw == WRITE && !bh->b_dirt) || (rw == READ && bh->b_uptodate)) {
		unlock_buffer(bh);
		return;
	}
repeat:
/* we don't allow the write-requests to fill up the queue completely:
 * we want some room for reads: they take precedence. The last third
 * of the requests are only for reads.
 */
	if (rw == READ)
		req = request+NR_REQUEST;
	else
		req = request+((NR_REQUEST*2)/3);
/* find an empty request */
	while (--req >= request)
		if (req->dev<0)
			break;
/* if none found, sleep on new requests: check for rw_ahead */
	if (req < request) {
		if (rw_ahead) {
			unlock_buffer(bh);
			return;
		}
		sleep_on(&wait_for_request);
		goto repeat;
	}
/* fill up the request-info, and add it to the queue */
	req->dev = bh->b_dev;
	req->cmd = rw;
	req->errors=0;
	req->sector = bh->b_blocknr<<1;
	req->nr_sectors = 2;
	req->buffer = bh->b_data;
	req->waiting = NULL;
	req->bh = bh;
	req->next = NULL;
	add_request(major+blk_dev,req);
}

void ll_rw_block(int rw, struct buffer_head * bh)
{
	unsigned int major;

	if ((major=MAJOR(bh->b_dev)) >= NR_BLK_DEV ||
	!(blk_dev[major].request_fn)) {
		printk("Trying to read nonexistent block-device\n\r");
		return;
	}
	make_request(major,rw,bh);
}

void blk_dev_init(void)
{
	int i;

	for (i=0 ; i<NR_REQUEST ; i++) {
		request[i].dev = -1;
		request[i].next = NULL;
	}
}
