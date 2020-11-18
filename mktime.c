/*
 *  linux/kernel/mktime.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <time.h>

/*
 * This isn't the library routine, it is only used in the kernel.
 * as such, we don't care about years<1970 etc, but assume everything
 * is ok. Similarly, TZ etc is happily ignored. We just do everything
 * as easily as possible. Let's find something public for the library
 * routines (although I think minix times is public).
 * 
 * 11.分析copy_page_tables（）函数的代码，叙述父进程如何为子进程复制页表。
进入copy_page_table函数后，先为新的页表申请一个空闲页面，并把进程0中的第一个页表里面前160个页表项复制到这个页面中（1个页表项控制一个页面4KB内存空间，160个页表项可以控制640KB内存空间）。进程0和进程1的页表暂时都指向了相同的页面，意味着进程1也可以操作进程0的页面，之后对进程1的页目录表进行设置。最后，用重置CR3的方法刷新页变换高速缓存。进程1的页表和页目录表设置完毕。（P96）



12.进程0创建进程1时，为进程1建立了task_struct及内核栈，第一个页表，分别位于物理内存16MB顶端倒数第一页、第二页。请问，这两个页究竟占用的是谁的线性地址空间，内核、进程0、进程1、还是没有占用任何线性地址空间？说明理由（可以图示）并给出代码证据。
这两个页占用的是内核的线性地址空间，依据在
setup_paging(文件head.s)中， 
movl $pg3+4092,%edi
movl $0xfff007,%eax /* 16Mb -4096 + 7 (r/w user,p) */
std 
1: stosl/* fill pages backwards -more efficient :-) */ subl $0x1000,%eax 
上面的代码，指明了内核的线性地址空间为0x000000 ~ 0xffffff(即前16M)，且线性地址与 物理地址呈现一一对应的关系。为进程1分配的这两个页，在16MB的顶端倒数第一页、第 二页，因此占用内核的线性地址空间。进程 0 的线性地址空间是内存前640KB，因为进程 0 的 LDT 中的 limit 属性限制了进程 0 能够访问的地址空间。进程 1 拷贝了进程 0 的页表(160 项)，而这 160 个页表项即为内核 
第一个页表的前 160 项，指向的是物理内存前 640KB，因此无法访问到 16MB 的顶端倒数 的两个页。 

 */
/*
 * PS. I hate whoever though up the year 1970 - couldn't they have gotten
 * a leap-year instead? I also hate Gregorius, pope or no. I'm grumpy.
 */
#define MINUTE 60
#define HOUR (60*MINUTE)
#define DAY (24*HOUR)
#define YEAR (365*DAY)

/* interestingly, we assume leap-years */
static int month[12] = {
	0,
	DAY*(31),
	DAY*(31+29),
	DAY*(31+29+31),
	DAY*(31+29+31+30),
	DAY*(31+29+31+30+31),
	DAY*(31+29+31+30+31+30),
	DAY*(31+29+31+30+31+30+31),
	DAY*(31+29+31+30+31+30+31+31),
	DAY*(31+29+31+30+31+30+31+31+30),
	DAY*(31+29+31+30+31+30+31+31+30+31),
	DAY*(31+29+31+30+31+30+31+31+30+31+30)
};

long kernel_mktime(struct tm * tm)
{
	long res;
	int year;

	year = tm->tm_year - 70;
/* magic offsets (y+1) needed to get leapyears right.*/
	res = YEAR*year + DAY*((year+1)/4);
	res += month[tm->tm_mon];
/* and (y+2) here. If it wasn't a leap-year, we have to adjust */
	if (tm->tm_mon>1 && ((year+2)%4))
		res -= DAY;
	res += DAY*(tm->tm_mday-1);
	res += HOUR*tm->tm_hour;
	res += MINUTE*tm->tm_min;
	res += tm->tm_sec;
	return res;
}
