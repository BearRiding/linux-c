/*
 * malloc.c --- a general purpose kernel memory allocator for Linux.
 * 
 * Written by Theodore Ts'o (tytso@mit.edu), 11/29/91
 *
 * This routine is written to be as fast as possible, so that it
 * can be called from the interrupt level.
 *
 * Limitations: maximum size of memory we can allocate using this routine
 *	is 4k, the size of a page in Linux.
 *
 * The general game plan is that each page (called a bucket) will only hold
 * objects of a given size.  When all of the object on a page are released,
 * the page can be returned to the general free pool.  When malloc() is
 * called, it looks for the smallest bucket size which will fulfill its
 * request, and allocate a piece of memory from that bucket pool.
 *
 * Each bucket has as its control block a bucket descriptor which keeps 
 * track of how many objects are in use on that page, and the free list
 * for that page.  Like the buckets themselves, bucket descriptors are
 * stored on pages requested from get_free_page().  However, unlike buckets,
 * pages devoted to bucket descriptor pages are never released back to the
 * system.  Fortunately, a system should probably only need 1 or 2 bucket
 * descriptor pages, since a page can hold 256 bucket descriptors (which
 * corresponds to 1 megabyte worth of bucket pages.)  If the kernel is using 
 * that much allocated memory, it's probably doing something wrong.  :-)
 *
 * Note: malloc() and free() both call get_free_page() and free_page()
 *	in sections of code where interrupts are turned off, to allow
 *	malloc() and free() to be safely called from an interrupt routine.
 *	(We will probably need this functionality when networking code,
 *	particularily things like NFS, is added to Linux.)  However, this
 *	presumes that get_free_page() and free_page() are interrupt-level
 *	safe, which they may not be once paging is added.  If this is the
 *	case, we will need to modify malloc() to keep a few unused pages
 *	"pre-allocated" so that it can safely draw upon those pages if
 * 	it is called from an interrupt routine.
 *
 * 	Another concern is that get_free_page() should not sleep; if it 
 *	does, the code is carefully ordered so as to avoid any race 
 *	conditions.  The catch is that if malloc() is called re-entrantly, 
 *	there is a chance that unecessary pages will be grabbed from the 
 *	system.  Except for the pages for the bucket descriptor page, the 
 *	extra pages will eventually get released back to the system, though,
 *	so it isn't all that bad.

 4.在 IA-32 中，有大约 20 多个指令是只能在 0 特权级下使用，其他的指令，比如 cli，并没 有这个约定。奇怪的是，在 Linux0.11 中，在 3 特权级的进程代码并不能使用 cli 指令，会 报特权级错误，这是为什么？请解释并给出代码证据。
根据Intel Manual，cli和sti指令与CPL和EFLAGS[IOPL]有关。
通过IOPL来加以保护指令in,ins,out,outs,cli,sti等I/O敏感指令，只有CPL(当前特权级)<=IOPL才能执行，低特权级访问这些指令将会产生一个一般性保护异常。
IOPL位于EFLAGS的12-13位，仅可通过iret来改变，INIT_TASK中IOPL为0，在move_to_user_mode中直接执行“pushfl \n\t”指令，继承了内核的EFLAGS。IOPL的指令仍然为0没有改变，所以用户进程无法调用cli指令。因此，通过设置 IOPL， 3特权级的进程代码不能使用 cli 等I/O敏感指令。

CLI：如果 CPL 的权限高于等于 eflags 中的 IOPL 的权限，即数值上： cpl <= IOPL，则 IF 位清除为 0；否则它不受影响。 EFLAGS 寄存器中的其他标志不受影响。#GP(0) –如果 CPL 大于（特权更小）当前程序或过程的 IOPL，产生保护模式异常。
由于在内核 IOPL 的值初始时为 0，且未经改变。进程 0 在 move_to_user_mode 中，继承了内核的 eflags。
move_to_user_mode()
…
"pushfl\n\t" \
…
"iret\n" \
而进程 1 再 copy_process 中，在进程的 TSS 中，设置了 eflags 中的 IOPL 位为 0。总之，通过设置 IOPL，可以限制 3 特权级的进程代码使用 cli

6. 在system.h里
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
读懂代码。这里中断门、陷阱门、系统调用都是通过_set_gate设置的，用的是同一个嵌入汇编代码，比较明显的差别是dpl一个是3，另外两个是0，这是为什么？说明理由。
答：
当用户程序产生系统调用软中断后， 系统都通过system_call总入口找到具体的系统调用函数。 set_system_gate设置系统调用，须将 DPL设置为 3，允许在用户特权级（3）的进程调用，否则会引发 General Protection 异常。set_trap_gate 及 set_intr_gate 设置陷阱和中断为内核使用，需禁止用户进程调用，所以 DPL为 0。

 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/system.h>

struct bucket_desc {	/* 16 bytes */
	void			*page;
	struct bucket_desc	*next;
	void			*freeptr;
	unsigned short		refcnt;
	unsigned short		bucket_size;
};

struct _bucket_dir {	/* 8 bytes */
	int			size;
	struct bucket_desc	*chain;
};

/*
 * The following is the where we store a pointer to the first bucket
 * descriptor for a given size.  
 *
 * If it turns out that the Linux kernel allocates a lot of objects of a
 * specific size, then we may want to add that specific size to this list,
 * since that will allow the memory to be allocated more efficiently.
 * However, since an entire page must be dedicated to each specific size
 * on this list, some amount of temperance must be exercised here.
 *
 * Note that this list *must* be kept in order.
 */
struct _bucket_dir bucket_dir[] = {
	{ 16,	(struct bucket_desc *) 0},
	{ 32,	(struct bucket_desc *) 0},
	{ 64,	(struct bucket_desc *) 0},
	{ 128,	(struct bucket_desc *) 0},
	{ 256,	(struct bucket_desc *) 0},
	{ 512,	(struct bucket_desc *) 0},
	{ 1024,	(struct bucket_desc *) 0},
	{ 2048, (struct bucket_desc *) 0},
	{ 4096, (struct bucket_desc *) 0},
	{ 0,    (struct bucket_desc *) 0}};   /* End of list marker */

/*
 * This contains a linked list of free bucket descriptor blocks
 */
struct bucket_desc *free_bucket_desc = (struct bucket_desc *) 0;

/*
 * This routine initializes a bucket description page.
 */
static inline void init_bucket_desc()
{
	struct bucket_desc *bdesc, *first;
	int	i;
	
	first = bdesc = (struct bucket_desc *) get_free_page();
	if (!bdesc)
		panic("Out of memory in init_bucket_desc()");
	for (i = PAGE_SIZE/sizeof(struct bucket_desc); i > 1; i--) {
		bdesc->next = bdesc+1;
		bdesc++;
	}
	/*
	 * This is done last, to avoid race conditions in case 
	 * get_free_page() sleeps and this routine gets called again....
	 */
	bdesc->next = free_bucket_desc;
	free_bucket_desc = first;
}

void *malloc(unsigned int len)
{
	struct _bucket_dir	*bdir;
	struct bucket_desc	*bdesc;
	void			*retval;

	/*
	 * First we search the bucket_dir to find the right bucket change
	 * for this request.
	 */
	for (bdir = bucket_dir; bdir->size; bdir++)
		if (bdir->size >= len)
			break;
	if (!bdir->size) {
		printk("malloc called with impossibly large argument (%d)\n",
			len);
		panic("malloc: bad arg");
	}
	/*
	 * Now we search for a bucket descriptor which has free space
	 */
	cli();	/* Avoid race conditions */
	for (bdesc = bdir->chain; bdesc; bdesc = bdesc->next) 
		if (bdesc->freeptr)
			break;
	/*
	 * If we didn't find a bucket with free space, then we'll 
	 * allocate a new one.
	 */
	if (!bdesc) {
		char		*cp;
		int		i;

		if (!free_bucket_desc)	
			init_bucket_desc();
		bdesc = free_bucket_desc;
		free_bucket_desc = bdesc->next;
		bdesc->refcnt = 0;
		bdesc->bucket_size = bdir->size;
		bdesc->page = bdesc->freeptr = (void *) cp = get_free_page();
		if (!cp)
			panic("Out of memory in kernel malloc()");
		/* Set up the chain of free objects */
		for (i=PAGE_SIZE/bdir->size; i > 1; i--) {
			*((char **) cp) = cp + bdir->size;
			cp += bdir->size;
		}
		*((char **) cp) = 0;
		bdesc->next = bdir->chain; /* OK, link it in! */
		bdir->chain = bdesc;
	}
	retval = (void *) bdesc->freeptr;
	bdesc->freeptr = *((void **) retval);
	bdesc->refcnt++;
	sti();	/* OK, we're safe again */
	return(retval);
}

/*
 * Here is the free routine.  If you know the size of the object that you
 * are freeing, then free_s() will use that information to speed up the
 * search for the bucket descriptor.
 * 
 * We will #define a macro so that "free(x)" is becomes "free_s(x, 0)"
 */
void free_s(void *obj, int size)
{
	void		*page;
	struct _bucket_dir	*bdir;
	struct bucket_desc	*bdesc, *prev;

	/* Calculate what page this object lives in */
	page = (void *)  ((unsigned long) obj & 0xfffff000);
	/* Now search the buckets looking for that page */
	for (bdir = bucket_dir; bdir->size; bdir++) {
		prev = 0;
		/* If size is zero then this conditional is always false */
		if (bdir->size < size)
			continue;
		for (bdesc = bdir->chain; bdesc; bdesc = bdesc->next) {
			if (bdesc->page == page) 
				goto found;
			prev = bdesc;
		}
	}
	panic("Bad address passed to kernel free_s()");
found:
	cli(); /* To avoid race conditions */
	*((void **)obj) = bdesc->freeptr;
	bdesc->freeptr = obj;
	bdesc->refcnt--;
	if (bdesc->refcnt == 0) {
		/*
		 * We need to make sure that prev is still accurate.  It
		 * may not be, if someone rudely interrupted us....
		 */
		if ((prev && (prev->next != bdesc)) ||
		    (!prev && (bdir->chain != bdesc)))
			for (prev = bdir->chain; prev; prev = prev->next)
				if (prev->next == bdesc)
					break;
		if (prev)
			prev->next = bdesc->next;
		else {
			if (bdir->chain != bdesc)
				panic("malloc bucket chains corrupted");
			bdir->chain = bdesc->next;
		}
		free_page((unsigned long) bdesc->page);
		bdesc->next = free_bucket_desc;
		free_bucket_desc = bdesc;
	}
	sti();
	return;
}

