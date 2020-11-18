/*
 *  linux/fs/stat.c
 *
 *  (C) 1991  Linus Torvalds
 * 
 * 8.为什么 static inline _syscall0(type,name)中需要加上关键字 inline？
因为_syscall0(int,fork)展开是一个真函数，普通真函数调用事需要将 eip 入栈，返回时需要 讲 eip 出栈。inline 是内联函数，它将标明为 inline 的函数代码放在符号表中，而此处的 fork 函数需要调用两次，加上 inline 后先进行词法分析、语法分析正确后就地展开函数，不需要 有普通函数的 call\ret 等指令，也不需要保持栈的 eip，效率很高。若不加上 inline，第一次 调用 fork 结束时将 eip 出栈，第二次调用返回的 eip 出栈值将是一个错误值。
答案2：inline一般是用于定义内联函数，内联函数结合了函数以及宏的优点，在定义时和函数一样，编译器会对其参数进行检查；在使用时和宏类似，内联函数的代码会被直接嵌入在它被调用的地方，这样省去了函数调用时的一些额外开销，比如保存和恢复函数返回地址等，可以加快速度。

9、打开保护模式、分页后，线性地址到物理地址是如何转换的？
答：保护模式下，每个线性地址为32位，MMU按照10-10-12的长度来识别线性地址的值。CR3中存储着页目录表的基址，线性地址的前10位表示页目录表中的页目录项，由此得到所在的页表地址。中间10位记录了页表中的页表项位置，由此得到页的位置，最后12位表示页内偏移。示意图（P97 图3-9线性地址到物理地址映射过程示意图）

在保护模式下，先行地址到物理地址的转化是通过内存分页管理机制实现的。其基本原理是将整个线性和物理内存区域划分为 4K 大小的内存页面，系统以页为单位进行分配和回收。每个线性地址为 32 位， MMU 按照 10-10-12 的长度来识别线性地址的值。CR3 中存储着页目录表的基址，线性地址的前十位表示也目录表中的页目录项，由此得到所在的页表地址。21~12 位记录了页表中的页表项位置，由此得到页的位置，最后 12 位表示页内偏移。

 */

#include <errno.h>
#include <sys/stat.h>

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

static void cp_stat(struct m_inode * inode, struct stat * statbuf)
{
	struct stat tmp;
	int i;

	verify_area(statbuf,sizeof (* statbuf));
	tmp.st_dev = inode->i_dev;
	tmp.st_ino = inode->i_num;
	tmp.st_mode = inode->i_mode;
	tmp.st_nlink = inode->i_nlinks;
	tmp.st_uid = inode->i_uid;
	tmp.st_gid = inode->i_gid;
	tmp.st_rdev = inode->i_zone[0];
	tmp.st_size = inode->i_size;
	tmp.st_atime = inode->i_atime;
	tmp.st_mtime = inode->i_mtime;
	tmp.st_ctime = inode->i_ctime;
	for (i=0 ; i<sizeof (tmp) ; i++)
		put_fs_byte(((char *) &tmp)[i],&((char *) statbuf)[i]);
}

int sys_stat(char * filename, struct stat * statbuf)
{
	struct m_inode * inode;

	if (!(inode=namei(filename)))
		return -ENOENT;
	cp_stat(inode,statbuf);
	iput(inode);
	return 0;
}

int sys_fstat(unsigned int fd, struct stat * statbuf)
{
	struct file * f;
	struct m_inode * inode;

	if (fd >= NR_OPEN || !(f=current->filp[fd]) || !(inode=f->f_inode))
		return -EBADF;
	cp_stat(inode,statbuf);
	return 0;
}
