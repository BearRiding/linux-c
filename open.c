/*
 *  linux/lib/open.c
 * 
 * 17、wait_on_buffer函数中为什么不用if（）而是用while（）？
答：因为可能存在一种情况是，很多进程都在等待一个缓冲块。在缓冲块同步完毕，唤醒各等待进程到轮转到某一进程的过程中，很有可能此时的缓冲块又被其它进程所占用，并被加上了锁。此时如果用if()，则此进程会从之前被挂起的地方继续执行，不会再判断是否缓冲块已被占用而直接使用，就会出现错误；而如果用while()，则此进程会再次确认缓冲块是否已被占用，在确认未被占用后，才会使用，这样就不会发生之前那样的错误。


18、getblk函数中，申请空闲缓冲块的标准就是b_count为0，而申请到之后，为什么在wait_on_buffer(bh)后又执行if（bh->b_count）来判断b_count是否为0？
P114参考，考试不用看
wait_on_buffer(bh)内包含睡眠函数，虽然此时已经找到比较合适的空闲缓冲块，但是可能在睡眠阶段该缓冲区被其他任务所占用，因此必须重新搜索，判断是否被修改，修改则写盘等待解锁。判断若被占用则重新repeat，继续执行if（bh->b_count）


 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>
#include <stdarg.h>

int open(const char * filename, int flag, ...)
{
	register int res;
	va_list arg;

	va_start(arg,flag);
	__asm__("int $0x80"
		:"=a" (res)
		:"0" (__NR_open),"b" (filename),"c" (flag),
		"d" (va_arg(arg,int)));
	if (res>=0)
		return res;
	errno = -res;
	return -1;
}
