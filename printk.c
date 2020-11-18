/*
 *  linux/kernel/printk.c
 *
 *  (C) 1991  Linus Torvalds
 * 
 * 23、操作系统如何利用b_uptodate保证缓冲块数据的正确性？new_block (int dev)函数新申请一个缓冲块后，并没有读盘，b_uptodate却被置1，是否会引起数据混乱？详细分析理由。
b_uptodate 是缓冲块中针对进程方向的标志位，它的作用是告诉内核，缓冲块的数据是否已是数据块中最新的。当 b_update 置 1 时，就说明缓冲块中的数据是基于硬盘数据块的，内核可以放心地支持进程与缓冲块进行数据交互；如果 b_uptodate 为 0，就提醒内核缓冲块并没有用绑定的数据块中的数据更新，不支持进程共享该缓冲块。
当为文件创建新数据块，新建一个缓冲块时， b_uptodate 被置 1，但并不会引起数据混乱。此时，新建的数据块只可能有两个用途，一个是存储文件内容，一个是存储文件的i_zone 的间接块管理信息。
如果是存储文件内容，由于新建数据块和新建硬盘数据块，此时都是垃圾数据，都不是硬盘所需要的，无所谓数据是否更新，结果“等效于”更新问题已经解决。
如果是存储文件的间接块管理信息，必须清零，表示没有索引间接数据块，否则垃圾数据会导致索引错误，破坏文件操作的正确性。虽然缓冲块与硬盘数据块的数据不一致，但同样将 b_uptodate 置 1 不会有问题。
综合以上考虑，设计者采用的策略是，只要为新建的数据块新申请了缓冲块，不管这个缓冲块将来用作什么，反正进程现在不需要里面的数据，干脆全部清零。这样不管与之绑定的数据块用来存储什么信息，都无所谓，将该缓冲块的 b_uptodate 字段设置为 1，更新问题“等效于”已解决

 */

/*
 * When in kernel-mode, we cannot use printf, as fs is liable to
 * point to 'interesting' things. Make a printf with fs-saving, and
 * all is well.
 */
#include <stdarg.h>
#include <stddef.h>

#include <linux/kernel.h>

static char buf[1024];

extern int vsprintf(char * buf, const char * fmt, va_list args);

int printk(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i=vsprintf(buf,fmt,args);
	va_end(args);
	__asm__("push %%fs\n\t"
		"push %%ds\n\t"
		"pop %%fs\n\t"
		"pushl %0\n\t"
		"pushl $_buf\n\t"
		"pushl $0\n\t"
		"call _tty_write\n\t"
		"addl $8,%%esp\n\t"
		"popl %0\n\t"
		"pop %%fs"
		::"r" (i):"ax","cx","dx");
	return i;
}
