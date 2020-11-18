/*
 *  linux/kernel/panic.c
 *
 *  (C) 1991  Linus Torvalds
 * 
 * 19、b_dirt已经被置为1的缓冲块，同步前能够被进程继续读、写？给出代码证据。
同步前能够被进程继续读、写 （但不能挪为它用（即关联其它物理块））
b_uptodate设置为1后，内核就可以支持进程共享该缓冲块的数据了，读写都可以，读操作不会改变缓冲块的内容，所以不影响数据，而执行写操作后，就改变了缓冲块的内容，就要将b_dirt标志设置为1。由于此前缓冲块中的数据已经用硬盘数据块更新了，所以后续的同步未被改写的部分不受影响，同步是不更改缓冲块中数据的，所以b_uptodate仍为1。即进程在b_dirt置为1时，仍能对缓冲区数据进行读写。
\linux0.11\fs\file_dev.c
int file_write(struct m_inode * inode, struct file * filp, char * buf, int count)
{
	//…
	if (filp->f_flags & O_APPEND)
		pos = inode->i_size;
	else
		pos = filp->f_pos;
	while (i<count) {
		if (!(block = create_block(inode,pos/BLOCK_SIZE)))
			break;
		if (!(bh=bread(inode->i_dev,block)))
			break;
	//…
}
int file_read(struct m_inode * inode, struct file * filp, char * buf, int count)
{
	//…
	if ((left=count)<=0)
		return 0;
	while (left) {
		if (nr = bmap(inode,(filp->f_pos)/BLOCK_SIZE)) {
			if (!(bh=bread(inode->i_dev,nr)))
				break;
		} 
	//…
}


20、分析panic函数的源代码，根据你学过的操作系统知识，完整、准确的判断panic函数所起的作用。假如操作系统设计为支持内核进程（始终运行在0特权级的进程），你将如何改进panic函数？
panic()函数是当系统发现无法继续运行下去的故障时将调用它，会导致程序终止，然后由系统显示错误号。如果出现错误的函数不是进程0，那么就要进行数据同步，把缓冲区中的数据尽量同步到硬盘上。遵循了Linux尽量简明的原则。
改进panic函数：将死循环for(;;)；改进为跳转到内核进程（始终运行在0特权级的进程），让内核继续执行。
代码： kernel/panic.c
#include <linux/kernel.h>
#include <linux/sched.h>
void sys_sync(void);
volatile void panic(const char * s)
{
         printk("Kernel panic: %s\n\r",s);
         if (current == task[0])
                   printk("In swapper task - not syncing\n\r");
         else
                   sys_sync();
         for(;;);
}

 */

/*
 * This function is used through-out the kernel (includeinh mm and fs)
 * to indicate a major problem.
 */
#include <linux/kernel.h>
#include <linux/sched.h>

void sys_sync(void);	/* it's really int */

volatile void panic(const char * s)
{
	printk("Kernel panic: %s\n\r",s);
	if (current == task[0])
		printk("In swapper task - not syncing\n\r");
	else
		sys_sync();
	for(;;);
}
