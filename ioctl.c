/*
 *  linux/fs/ioctl.c
 *
 *  (C) 1991  Linus Torvalds
 * 12.特权级的目的和意义是什么？为什么特权级是基于段的？
目的：在于保护高特权级的段，其中操作系统的内核处于最高的特权级。 
意义：保护模式中的特权级，对操作系统的“主奴机制”影响深远。
在操作系统设计中，一个段一般实现的功能相对完整，可以把代码放在一个段，
数据放在一 个段，并通过段选择符（包括 CS、SS、DS、ES、FS 和 GS）获取段的基址和特权级等信息。 
特权级基于段，这样当段选择子具有不匹 配的特权级时，按照特权级规则评判是否可以访 问。
特权级基于段，是结合了程序的特点和硬件 实现的一种考虑。
通过段，系统划分了内核代码段、内核数据段、用户代码段和用户数据段等不同的数据段，
有些段是系统专享的，有些是和用户程序共享的，因此就有特权级的概念。

 */

#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <linux/sched.h>

extern int tty_ioctl(int dev, int cmd, int arg);

typedef int (*ioctl_ptr)(int dev,int cmd,int arg);

#define NRDEVS ((sizeof (ioctl_table))/(sizeof (ioctl_ptr)))

static ioctl_ptr ioctl_table[]={
	NULL,		/* nodev */
	NULL,		/* /dev/mem */
	NULL,		/* /dev/fd */
	NULL,		/* /dev/hd */
	tty_ioctl,	/* /dev/ttyx */
	tty_ioctl,	/* /dev/tty */
	NULL,		/* /dev/lp */
	NULL};		/* named pipes */
	

int sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{	
	struct file * filp;
	int dev,mode;

	if (fd >= NR_OPEN || !(filp = current->filp[fd]))
		return -EBADF;
	mode=filp->f_inode->i_mode;
	if (!S_ISCHR(mode) && !S_ISBLK(mode))
		return -EINVAL;
	dev = filp->f_inode->i_zone[0];
	if (MAJOR(dev) >= NRDEVS)
		return -ENODEV;
	if (!ioctl_table[MAJOR(dev)])
		return -ENOTTY;
	return ioctl_table[MAJOR(dev)](dev,cmd,arg);
}
