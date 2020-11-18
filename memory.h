/*
 *  NOTE!!! memcpy(dest,src,n) assumes ds=es=normal data segment. This
 *  goes for all kernel functions (ds=es=kernel space, fs=local data,
 *  gs=null), as well as for all well-behaving user programs (ds=es=
 *  user data space). This is NOT a bug, as any user program that changes
 *  es deserves to die if it isn't careful.
 * 10.分析get_free_page()函数的代码，叙述在主内存中获取一个空闲页的技术路线。
通过逆向扫描页表位图 mem_map， 并由第一空页的下标左移 12 位加 LOW_MEM 得到该页的物理地址， 位于 16M 内存末端。P89代码考试不用看
过程：
① 将EAX 设置为0,EDI 设置指向mem_map 的最后一项（mem_map+PAGING_PAGES-1），std设置扫描是从高地址向低地址。从mem_map的最后一项反向扫描，找出引用次数为0(AL)的页，如果没有则退出；如果找到，则将找到的页设引用数为1；
② ECX左移12位得到页的相对地址，加LOW_MEM得到物理地址，将此页最后一个字节的地址赋值给EDI（LOW_MEM+4092）；
③ stosl将EAX的值设置到ES:EDI所指内存，即反向清零1024*32bit，将此页清空；
④ 将页的地址（存放在EAX）返回。
代码如下：
unsigned long get_free_page(void)
64 {
65 register unsigned long __res asm("ax");
66
67 __asm__("std ; repne ; scasb\n\t" //反向扫描串,al(0)与 di 不等则重复
68 "jne 1f\n\t" //找不到空闲页跳转 1
69 "movb $1,1(%%edi)\n\t" //将 1 付给 edi+1 的位置，在 mem_map 中将找到 0 的项引用计
------------------------------------------数置为 1
70 "sall $12,%%ecx\n\t" //ecx 算数左移 12 位，页的相对地址
71 "addl %2,%%ecx\n\t" //LOW MEN +ecx 页物理地址
72 "movl %%ecx,%%edx\n\t"
73 "movl $1024,%%ecx\n\t"
74 "leal 4092(%%edx),%%edi\n\t" //将 edx+4kb 的有效地址赋给 edi
75 "rep ; stosl\n\t" //将 eax 赋给 edi 指向的地址，目的是页面清零。
76 " movl %%edx,%%eax\n"
77 "1: cld"
78 :"=a" (__res)
79 :"0" (0),"i" (LOW_MEM),"c" (PAGING_PAGES),
80 "D" (mem_map+PAGING_PAGES-1) //edx,mem_map[]的嘴鸥一个元素
81 );
82 return __res;
83 }

 */
#define memcpy(dest,src,n) ({ \
void * _res = dest; \
__asm__ ("cld;rep;movsb" \
	::"D" ((long)(_res)),"S" ((long)(src)),"c" ((long) (n)) \
	:"di","si","cx"); \
_res; \
})
