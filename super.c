/*
 *  linux/fs/super.c
 *
 *  (C) 1991  Linus Torvalds
 * 
 * 14、保护模式在“保护”什么？它的“保护”体现在哪里？特权级的目的和意义是什么？分页有“保护”作用吗？p438-440
（1） 保护模式在“保护”什么？它的“保护”体现在哪里？
保护操作系统的安全，不受到恶意攻击。保护进程地址空间。
“保护”体现在
打开保护模式后，CPU 的寻址模式发生了变化，基于 GDT 去获取代码或数据段的基址，相当于增加了一个段位寄存器。防止了对代码或数据段的覆盖以及代码段自身的访问超限。对描述符所描述的对象进行保护：在 GDT、 LDT 及 IDT 中，均有对应界限、特权级等；在不同特权级间访问时，系统会对 CPL、 RPL、 DPL、 IOPL 等进行检验，同时限制某些特殊指令如 lgdt, lidt,cli 等的使用；分页机制中 PDE 和 PTE 中的 R/W 和 U/S 等提供了页级保护，分页机制通过将线性地址与物理地址的映射，提供了对物理地址的保护。
（2）特权级的目的和意义是什么？
特权级机制目的是为了进行合理的管理资源，保护高特权级的段。
意义是进行了对系统的保护，对操作系统的“主奴机制”影响深远。Intel 从硬件上禁止低特权级代码段使用部分关键性指令，通过特权级的设置禁止用户进程使用 cli、 sti 等指令。将内核设计成最高特权级，用户进程成为最低特权级。这样，操作系统可以访问 GDT、 LDT、 TR，而 GDT、 LDT 是逻辑地址形成线性地址的关键，因此操作系统可以掌控线性地址。物理地址是由内核将线性地址转换而成的，所以操作系统可以访问任何物理地址。而用户进程只能使用逻辑地址。总之，特权级的引入对操作系统内核进行保护。
（3）分页有“保护”作用吗？
分页机制有保护作用，使得用户进程不能直接访问内核地址，进程间也不能相互访问。用户进程只能使用逻辑地址，而逻辑地址通过内核转化为线性地址，根据内核提供的专门为进程设计的分页方案，由MMU非直接映射转化为实际物理地址形成保护。此外，通过分页机制，每个进程都有自己的专属页表，有利于更安全、高效的使用内存，保护每个进程的地址空间。

 
15、在 head 程序执行结束的时候，在 idt 的前面有 184 个字节的 head 程序的剩余代码，剩余了什么？为什么要剩余？
剩余的内容： 0x5400~0x54b7 处包含了 after_page_tables 、 ignore_int 中断服务程序和 setup_paging 设 置分页的代码。
原因： after_page_tables 中压入了一些参数，为内核进入 main 函数的跳转做准备。为了谨 慎起见，设计者在栈中压入了 L6，以使得系统可能出错时，返回到 L6 处执行。ignore_int: 使 用 ignore_int 将 idt 全部初始化，因此如果中断开启后，可能使用了未设置的中断向量，那 么将默认跳转到 ignore_int 处执行。这样做的好处是使得系统不会跳转到随机的地方执行错 误的代码，所以 ignore_int 不能被覆盖。 setup_paging:为设置分页机制的代码，它在分页完 成前不能被覆盖

 */

/*
 * super.c contains code to handle the super-block tables.
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#include <errno.h>
#include <sys/stat.h>

int sync_dev(int dev);
void wait_for_keypress(void);

/* set_bit uses setb, as gas doesn't recognize setc */
#define set_bit(bitnr,addr) ({ \
register int __res __asm__("ax"); \
__asm__("bt %2,%3;setb %%al":"=a" (__res):"a" (0),"r" (bitnr),"m" (*(addr))); \
__res; })

struct super_block super_block[NR_SUPER];
/* this is initialized in init/main.c */
int ROOT_DEV = 0;

static void lock_super(struct super_block * sb)
{
	cli();
	while (sb->s_lock)
		sleep_on(&(sb->s_wait));
	sb->s_lock = 1;
	sti();
}

static void free_super(struct super_block * sb)
{
	cli();
	sb->s_lock = 0;
	wake_up(&(sb->s_wait));
	sti();
}

static void wait_on_super(struct super_block * sb)
{
	cli();
	while (sb->s_lock)
		sleep_on(&(sb->s_wait));
	sti();
}

struct super_block * get_super(int dev)
{
	struct super_block * s;

	if (!dev)
		return NULL;
	s = 0+super_block;
	while (s < NR_SUPER+super_block)
		if (s->s_dev == dev) {
			wait_on_super(s);
			if (s->s_dev == dev)
				return s;
			s = 0+super_block;
		} else
			s++;
	return NULL;
}

void put_super(int dev)
{
	struct super_block * sb;
	struct m_inode * inode;
	int i;

	if (dev == ROOT_DEV) {
		printk("root diskette changed: prepare for armageddon\n\r");
		return;
	}
	if (!(sb = get_super(dev)))
		return;
	if (sb->s_imount) {
		printk("Mounted disk changed - tssk, tssk\n\r");
		return;
	}
	lock_super(sb);
	sb->s_dev = 0;
	for(i=0;i<I_MAP_SLOTS;i++)
		brelse(sb->s_imap[i]);
	for(i=0;i<Z_MAP_SLOTS;i++)
		brelse(sb->s_zmap[i]);
	free_super(sb);
	return;
}

static struct super_block * read_super(int dev)
{
	struct super_block * s;
	struct buffer_head * bh;
	int i,block;

	if (!dev)
		return NULL;
	check_disk_change(dev);
	if (s = get_super(dev))
		return s;
	for (s = 0+super_block ;; s++) {
		if (s >= NR_SUPER+super_block)
			return NULL;
		if (!s->s_dev)
			break;
	}
	s->s_dev = dev;
	s->s_isup = NULL;
	s->s_imount = NULL;
	s->s_time = 0;
	s->s_rd_only = 0;
	s->s_dirt = 0;
	lock_super(s);
	if (!(bh = bread(dev,1))) {
		s->s_dev=0;
		free_super(s);
		return NULL;
	}
	*((struct d_super_block *) s) =
		*((struct d_super_block *) bh->b_data);
	brelse(bh);
	if (s->s_magic != SUPER_MAGIC) {
		s->s_dev = 0;
		free_super(s);
		return NULL;
	}
	for (i=0;i<I_MAP_SLOTS;i++)
		s->s_imap[i] = NULL;
	for (i=0;i<Z_MAP_SLOTS;i++)
		s->s_zmap[i] = NULL;
	block=2;
	for (i=0 ; i < s->s_imap_blocks ; i++)
		if (s->s_imap[i]=bread(dev,block))
			block++;
		else
			break;
	for (i=0 ; i < s->s_zmap_blocks ; i++)
		if (s->s_zmap[i]=bread(dev,block))
			block++;
		else
			break;
	if (block != 2+s->s_imap_blocks+s->s_zmap_blocks) {
		for(i=0;i<I_MAP_SLOTS;i++)
			brelse(s->s_imap[i]);
		for(i=0;i<Z_MAP_SLOTS;i++)
			brelse(s->s_zmap[i]);
		s->s_dev=0;
		free_super(s);
		return NULL;
	}
	s->s_imap[0]->b_data[0] |= 1;
	s->s_zmap[0]->b_data[0] |= 1;
	free_super(s);
	return s;
}

int sys_umount(char * dev_name)
{
	struct m_inode * inode;
	struct super_block * sb;
	int dev;

	if (!(inode=namei(dev_name)))
		return -ENOENT;
	dev = inode->i_zone[0];
	if (!S_ISBLK(inode->i_mode)) {
		iput(inode);
		return -ENOTBLK;
	}
	iput(inode);
	if (dev==ROOT_DEV)
		return -EBUSY;
	if (!(sb=get_super(dev)) || !(sb->s_imount))
		return -ENOENT;
	if (!sb->s_imount->i_mount)
		printk("Mounted inode has i_mount=0\n");
	for (inode=inode_table+0 ; inode<inode_table+NR_INODE ; inode++)
		if (inode->i_dev==dev && inode->i_count)
				return -EBUSY;
	sb->s_imount->i_mount=0;
	iput(sb->s_imount);
	sb->s_imount = NULL;
	iput(sb->s_isup);
	sb->s_isup = NULL;
	put_super(dev);
	sync_dev(dev);
	return 0;
}

int sys_mount(char * dev_name, char * dir_name, int rw_flag)
{
	struct m_inode * dev_i, * dir_i;
	struct super_block * sb;
	int dev;

	if (!(dev_i=namei(dev_name)))
		return -ENOENT;
	dev = dev_i->i_zone[0];
	if (!S_ISBLK(dev_i->i_mode)) {
		iput(dev_i);
		return -EPERM;
	}
	iput(dev_i);
	if (!(dir_i=namei(dir_name)))
		return -ENOENT;
	if (dir_i->i_count != 1 || dir_i->i_num == ROOT_INO) {
		iput(dir_i);
		return -EBUSY;
	}
	if (!S_ISDIR(dir_i->i_mode)) {
		iput(dir_i);
		return -EPERM;
	}
	if (!(sb=read_super(dev))) {
		iput(dir_i);
		return -EBUSY;
	}
	if (sb->s_imount) {
		iput(dir_i);
		return -EBUSY;
	}
	if (dir_i->i_mount) {
		iput(dir_i);
		return -EPERM;
	}
	sb->s_imount=dir_i;
	dir_i->i_mount=1;
	dir_i->i_dirt=1;		/* NOTE! we don't iput(dir_i) */
	return 0;			/* we do that in umount */
}

void mount_root(void)
{
	int i,free;
	struct super_block * p;
	struct m_inode * mi;

	if (32 != sizeof (struct d_inode))
		panic("bad i-node size");
	for(i=0;i<NR_FILE;i++)
		file_table[i].f_count=0;
	if (MAJOR(ROOT_DEV) == 2) {
		printk("Insert root floppy and press ENTER");
		wait_for_keypress();
	}
	for(p = &super_block[0] ; p < &super_block[NR_SUPER] ; p++) {
		p->s_dev = 0;
		p->s_lock = 0;
		p->s_wait = NULL;
	}
	if (!(p=read_super(ROOT_DEV)))
		panic("Unable to mount root");
	if (!(mi=iget(ROOT_DEV,ROOT_INO)))
		panic("Unable to read root i-node");
	mi->i_count += 3 ;	/* NOTE! it is logically used 4 times, not 1 */
	p->s_isup = p->s_imount = mi;
	current->pwd = mi;
	current->root = mi;
	free=0;
	i=p->s_nzones;
	while (-- i >= 0)
		if (!set_bit(i&8191,p->s_zmap[i>>13]->b_data))
			free++;
	printk("%d/%d free blocks\n\r",free,p->s_nzones);
	free=0;
	i=p->s_ninodes+1;
	while (-- i >= 0)
		if (!set_bit(i&8191,p->s_imap[i>>13]->b_data))
			free++;
	printk("%d/%d free inodes\n\r",free,p->s_ninodes);
}
