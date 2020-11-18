/*
 *  linux/fs/namei.c
 *
 *  (C) 1991  Linus Torvalds
 * 
 * 15.进程0开始创建进程1，调用fork（），跟踪代码时我们发现，fork代码执行了两次，第一次，执行fork代码后，跳过init（）直接执行了for(;;) pause()，第二次执行fork代码后，执行了init（）。奇怪的是，我们在代码中并没有看到向转向fork的goto语句，也没有看到循环语句，是什么原因导致fork反复执行？请说明理由（可以图示），并给出代码证据。
主要涉及的代码位置如下：
Init/main.c 代码中P103 —— if 判断
Include/unistd.h 中P102 —— fork函数代码
进程1 TSS赋值，特别是 eip，eax 赋值
copy_process:
p->pid = last_pid;
…
p->tss.eip = eip;
p->tss.eflags = eflags;
p->tss.eax = 0;
…
p->tss.esp = esp;
…
p->tss.cs = cs & 0xffff;
p->tss.ss = ss & 0xffff;
…
p->state = TASK_RUNNING;
return last_pid;
原因
fork 为 inline 函数，其中调用了 sys_call0，产生 0x80 中断，将 ss, esp, eflags, cs, eip 压栈，其中 eip 为 int 0x80 的下一句的地址。在 copy_process 中，内核将进程 0 的 tss 复制得到进程 1 的 tss，并将进程 1 的 tss.eax 设为 0，而进程 0 中的 eax 为 1。在进程调度时 tss 中的值被恢复至相应寄存器中，包括 eip， eax 等。所以中断返回后，进程 0 和进程 1 均会从 int  0x80 的下一句开始执行，即 fork 执行了两次。
由于 eax 代表返回值，所以进程 0 和进程 1 会得到不同的返回值，在fork返回到进程0后，进程0判断返回值非 0，因此执行代码for(;;) pause();
在sys_pause函数中，内核设置了进程0的状态为 TASK_INTERRUPTIBLE，并进行进程调度。由于只有进程1处于就绪态，因此调度执行进程1的指令。由于进程1在TSS中设置了eip等寄存器的值，因此从 int 0x80 的下一条指令开始执行，且设定返回 eax 的值作为 fork 的返回值（值为 0），因此进程1执行了 init 的 函数。导致反复执行，主要是利用了两个系统调用 sys_fork 和 sys_pause 对进程状态的设置，以及利用了进程调度机制。

16、详细分析进程调度的全过程。考虑所有可能（signal、alarm除外）
1. 进程中有就绪进程，且时间片没有用完。
正常情况下，schedule()函数首先扫描任务数组。通过比较每个就绪（TASK_RUNNING）任务的运行时间递减滴答计数counter 的值来确定当前哪个进程运行的时间最少。哪一个的值大，就表示运行时间还不长，于是就选中该进程，最后调用switch_to()执行实际的进程切换操作
2. 进程中有就绪进程，但所有就绪进程时间片都用完（c=0）
如果此时所有处于TASK_RUNNING 状态进程的时间片都已经用完，系统就会根据每个进程的优先权值priority，对系统中所有进程（包括正在睡眠的进程）重新计算每个任务需要运行的时间片值counter。计算的公式是：
counter = counter + priority/2
然后 schdeule()函数重新扫描任务数组中所有处于TASK_RUNNING 状态，重复上述过程，直到选择出一个进程为止。最后调用switch_to()执行实际的进程切换操作。
3. 所有进程都不是就绪的c=-1
此时代码中的c=-1，next=0，跳出循环后，执行switch_to(0)，切换到进程0执行，因此所有进程都不是就绪的时候进程0执行。

 */

/*
 * Some corrections by tytso.
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <const.h>
#include <sys/stat.h>

#define ACC_MODE(x) ("\004\002\006\377"[(x)&O_ACCMODE])

/*
 * comment out this line if you want names > NAME_LEN chars to be
 * truncated. Else they will be disallowed.
 */
/* #define NO_TRUNCATE */

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

/*
 *	permission()
 *
 * is used to check for read/write/execute permissions on a file.
 * I don't know if we should look at just the euid or both euid and
 * uid, but that should be easily changed.
 */
static int permission(struct m_inode * inode,int mask)
{
	int mode = inode->i_mode;

/* special case: not even root can read/write a deleted file */
	if (inode->i_dev && !inode->i_nlinks)
		return 0;
	else if (current->euid==inode->i_uid)
		mode >>= 6;
	else if (current->egid==inode->i_gid)
		mode >>= 3;
	if (((mode & mask & 0007) == mask) || suser())
		return 1;
	return 0;
}

/*
 * ok, we cannot use strncmp, as the name is not in our data space.
 * Thus we'll have to use match. No big problem. Match also makes
 * some sanity tests.
 *
 * NOTE! unlike strncmp, match returns 1 for success, 0 for failure.
 */
static int match(int len,const char * name,struct dir_entry * de)
{
	register int same __asm__("ax");

	if (!de || !de->inode || len > NAME_LEN)
		return 0;
	if (len < NAME_LEN && de->name[len])
		return 0;
	__asm__("cld\n\t"
		"fs ; repe ; cmpsb\n\t"
		"setz %%al"
		:"=a" (same)
		:"0" (0),"S" ((long) name),"D" ((long) de->name),"c" (len)
		:"cx","di","si");
	return same;
}

/*
 *	find_entry()
 *
 * finds an entry in the specified directory with the wanted name. It
 * returns the cache buffer in which the entry was found, and the entry
 * itself (as a parameter - res_dir). It does NOT read the inode of the
 * entry - you'll have to do that yourself if you want to.
 *
 * This also takes care of the few special cases due to '..'-traversal
 * over a pseudo-root and a mount point.
 */
static struct buffer_head * find_entry(struct m_inode ** dir,
	const char * name, int namelen, struct dir_entry ** res_dir)
{
	int entries;
	int block,i;
	struct buffer_head * bh;
	struct dir_entry * de;
	struct super_block * sb;

#ifdef NO_TRUNCATE
	if (namelen > NAME_LEN)
		return NULL;
#else
	if (namelen > NAME_LEN)
		namelen = NAME_LEN;
#endif
	entries = (*dir)->i_size / (sizeof (struct dir_entry));
	*res_dir = NULL;
	if (!namelen)
		return NULL;
/* check for '..', as we might have to do some "magic" for it */
	if (namelen==2 && get_fs_byte(name)=='.' && get_fs_byte(name+1)=='.') {
/* '..' in a pseudo-root results in a faked '.' (just change namelen) */
		if ((*dir) == current->root)
			namelen=1;
		else if ((*dir)->i_num == ROOT_INO) {
/* '..' over a mount-point results in 'dir' being exchanged for the mounted
   directory-inode. NOTE! We set mounted, so that we can iput the new dir */
			sb=get_super((*dir)->i_dev);
			if (sb->s_imount) {
				iput(*dir);
				(*dir)=sb->s_imount;
				(*dir)->i_count++;
			}
		}
	}
	if (!(block = (*dir)->i_zone[0]))
		return NULL;
	if (!(bh = bread((*dir)->i_dev,block)))
		return NULL;
	i = 0;
	de = (struct dir_entry *) bh->b_data;
	while (i < entries) {
		if ((char *)de >= BLOCK_SIZE+bh->b_data) {
			brelse(bh);
			bh = NULL;
			if (!(block = bmap(*dir,i/DIR_ENTRIES_PER_BLOCK)) ||
			    !(bh = bread((*dir)->i_dev,block))) {
				i += DIR_ENTRIES_PER_BLOCK;
				continue;
			}
			de = (struct dir_entry *) bh->b_data;
		}
		if (match(namelen,name,de)) {
			*res_dir = de;
			return bh;
		}
		de++;
		i++;
	}
	brelse(bh);
	return NULL;
}

/*
 *	add_entry()
 *
 * adds a file entry to the specified directory, using the same
 * semantics as find_entry(). It returns NULL if it failed.
 *
 * NOTE!! The inode part of 'de' is left at 0 - which means you
 * may not sleep between calling this and putting something into
 * the entry, as someone else might have used it while you slept.
 */
static struct buffer_head * add_entry(struct m_inode * dir,
	const char * name, int namelen, struct dir_entry ** res_dir)
{
	int block,i;
	struct buffer_head * bh;
	struct dir_entry * de;

	*res_dir = NULL;
#ifdef NO_TRUNCATE
	if (namelen > NAME_LEN)
		return NULL;
#else
	if (namelen > NAME_LEN)
		namelen = NAME_LEN;
#endif
	if (!namelen)
		return NULL;
	if (!(block = dir->i_zone[0]))
		return NULL;
	if (!(bh = bread(dir->i_dev,block)))
		return NULL;
	i = 0;
	de = (struct dir_entry *) bh->b_data;
	while (1) {
		if ((char *)de >= BLOCK_SIZE+bh->b_data) {
			brelse(bh);
			bh = NULL;
			block = create_block(dir,i/DIR_ENTRIES_PER_BLOCK);
			if (!block)
				return NULL;
			if (!(bh = bread(dir->i_dev,block))) {
				i += DIR_ENTRIES_PER_BLOCK;
				continue;
			}
			de = (struct dir_entry *) bh->b_data;
		}
		if (i*sizeof(struct dir_entry) >= dir->i_size) {
			de->inode=0;
			dir->i_size = (i+1)*sizeof(struct dir_entry);
			dir->i_dirt = 1;
			dir->i_ctime = CURRENT_TIME;
		}
		if (!de->inode) {
			dir->i_mtime = CURRENT_TIME;
			for (i=0; i < NAME_LEN ; i++)
				de->name[i]=(i<namelen)?get_fs_byte(name+i):0;
			bh->b_dirt = 1;
			*res_dir = de;
			return bh;
		}
		de++;
		i++;
	}
	brelse(bh);
	return NULL;
}

/*
 *	get_dir()
 *
 * Getdir traverses the pathname until it hits the topmost directory.
 * It returns NULL on failure.
 */
static struct m_inode * get_dir(const char * pathname)
{
	char c;
	const char * thisname;
	struct m_inode * inode;
	struct buffer_head * bh;
	int namelen,inr,idev;
	struct dir_entry * de;

	if (!current->root || !current->root->i_count)
		panic("No root inode");
	if (!current->pwd || !current->pwd->i_count)
		panic("No cwd inode");
	if ((c=get_fs_byte(pathname))=='/') {
		inode = current->root;
		pathname++;
	} else if (c)
		inode = current->pwd;
	else
		return NULL;	/* empty name is bad */
	inode->i_count++;
	while (1) {
		thisname = pathname;
		if (!S_ISDIR(inode->i_mode) || !permission(inode,MAY_EXEC)) {
			iput(inode);
			return NULL;
		}
		for(namelen=0;(c=get_fs_byte(pathname++))&&(c!='/');namelen++)
			/* nothing */ ;
		if (!c)
			return inode;
		if (!(bh = find_entry(&inode,thisname,namelen,&de))) {
			iput(inode);
			return NULL;
		}
		inr = de->inode;
		idev = inode->i_dev;
		brelse(bh);
		iput(inode);
		if (!(inode = iget(idev,inr)))
			return NULL;
	}
}

/*
 *	dir_namei()
 *
 * dir_namei() returns the inode of the directory of the
 * specified name, and the name within that directory.
 */
static struct m_inode * dir_namei(const char * pathname,
	int * namelen, const char ** name)
{
	char c;
	const char * basename;
	struct m_inode * dir;

	if (!(dir = get_dir(pathname)))
		return NULL;
	basename = pathname;
	while (c=get_fs_byte(pathname++))
		if (c=='/')
			basename=pathname;
	*namelen = pathname-basename-1;
	*name = basename;
	return dir;
}

/*
 *	namei()
 *
 * is used by most simple commands to get the inode of a specified name.
 * Open, link etc use their own routines, but this is enough for things
 * like 'chmod' etc.
 */
struct m_inode * namei(const char * pathname)
{
	const char * basename;
	int inr,dev,namelen;
	struct m_inode * dir;
	struct buffer_head * bh;
	struct dir_entry * de;

	if (!(dir = dir_namei(pathname,&namelen,&basename)))
		return NULL;
	if (!namelen)			/* special case: '/usr/' etc */
		return dir;
	bh = find_entry(&dir,basename,namelen,&de);
	if (!bh) {
		iput(dir);
		return NULL;
	}
	inr = de->inode;
	dev = dir->i_dev;
	brelse(bh);
	iput(dir);
	dir=iget(dev,inr);
	if (dir) {
		dir->i_atime=CURRENT_TIME;
		dir->i_dirt=1;
	}
	return dir;
}

/*
 *	open_namei()
 *
 * namei for open - this is in fact almost the whole open-routine.
 */
int open_namei(const char * pathname, int flag, int mode,
	struct m_inode ** res_inode)
{
	const char * basename;
	int inr,dev,namelen;
	struct m_inode * dir, *inode;
	struct buffer_head * bh;
	struct dir_entry * de;

	if ((flag & O_TRUNC) && !(flag & O_ACCMODE))
		flag |= O_WRONLY;
	mode &= 0777 & ~current->umask;
	mode |= I_REGULAR;
	if (!(dir = dir_namei(pathname,&namelen,&basename)))
		return -ENOENT;
	if (!namelen) {			/* special case: '/usr/' etc */
		if (!(flag & (O_ACCMODE|O_CREAT|O_TRUNC))) {
			*res_inode=dir;
			return 0;
		}
		iput(dir);
		return -EISDIR;
	}
	bh = find_entry(&dir,basename,namelen,&de);
	if (!bh) {
		if (!(flag & O_CREAT)) {
			iput(dir);
			return -ENOENT;
		}
		if (!permission(dir,MAY_WRITE)) {
			iput(dir);
			return -EACCES;
		}
		inode = new_inode(dir->i_dev);
		if (!inode) {
			iput(dir);
			return -ENOSPC;
		}
		inode->i_uid = current->euid;
		inode->i_mode = mode;
		inode->i_dirt = 1;
		bh = add_entry(dir,basename,namelen,&de);
		if (!bh) {
			inode->i_nlinks--;
			iput(inode);
			iput(dir);
			return -ENOSPC;
		}
		de->inode = inode->i_num;
		bh->b_dirt = 1;
		brelse(bh);
		iput(dir);
		*res_inode = inode;
		return 0;
	}
	inr = de->inode;
	dev = dir->i_dev;
	brelse(bh);
	iput(dir);
	if (flag & O_EXCL)
		return -EEXIST;
	if (!(inode=iget(dev,inr)))
		return -EACCES;
	if ((S_ISDIR(inode->i_mode) && (flag & O_ACCMODE)) ||
	    !permission(inode,ACC_MODE(flag))) {
		iput(inode);
		return -EPERM;
	}
	inode->i_atime = CURRENT_TIME;
	if (flag & O_TRUNC)
		truncate(inode);
	*res_inode = inode;
	return 0;
}

int sys_mknod(const char * filename, int mode, int dev)
{
	const char * basename;
	int namelen;
	struct m_inode * dir, * inode;
	struct buffer_head * bh;
	struct dir_entry * de;
	
	if (!suser())
		return -EPERM;
	if (!(dir = dir_namei(filename,&namelen,&basename)))
		return -ENOENT;
	if (!namelen) {
		iput(dir);
		return -ENOENT;
	}
	if (!permission(dir,MAY_WRITE)) {
		iput(dir);
		return -EPERM;
	}
	bh = find_entry(&dir,basename,namelen,&de);
	if (bh) {
		brelse(bh);
		iput(dir);
		return -EEXIST;
	}
	inode = new_inode(dir->i_dev);
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}
	inode->i_mode = mode;
	if (S_ISBLK(mode) || S_ISCHR(mode))
		inode->i_zone[0] = dev;
	inode->i_mtime = inode->i_atime = CURRENT_TIME;
	inode->i_dirt = 1;
	bh = add_entry(dir,basename,namelen,&de);
	if (!bh) {
		iput(dir);
		inode->i_nlinks=0;
		iput(inode);
		return -ENOSPC;
	}
	de->inode = inode->i_num;
	bh->b_dirt = 1;
	iput(dir);
	iput(inode);
	brelse(bh);
	return 0;
}

int sys_mkdir(const char * pathname, int mode)
{
	const char * basename;
	int namelen;
	struct m_inode * dir, * inode;
	struct buffer_head * bh, *dir_block;
	struct dir_entry * de;

	if (!suser())
		return -EPERM;
	if (!(dir = dir_namei(pathname,&namelen,&basename)))
		return -ENOENT;
	if (!namelen) {
		iput(dir);
		return -ENOENT;
	}
	if (!permission(dir,MAY_WRITE)) {
		iput(dir);
		return -EPERM;
	}
	bh = find_entry(&dir,basename,namelen,&de);
	if (bh) {
		brelse(bh);
		iput(dir);
		return -EEXIST;
	}
	inode = new_inode(dir->i_dev);
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}
	inode->i_size = 32;
	inode->i_dirt = 1;
	inode->i_mtime = inode->i_atime = CURRENT_TIME;
	if (!(inode->i_zone[0]=new_block(inode->i_dev))) {
		iput(dir);
		inode->i_nlinks--;
		iput(inode);
		return -ENOSPC;
	}
	inode->i_dirt = 1;
	if (!(dir_block=bread(inode->i_dev,inode->i_zone[0]))) {
		iput(dir);
		free_block(inode->i_dev,inode->i_zone[0]);
		inode->i_nlinks--;
		iput(inode);
		return -ERROR;
	}
	de = (struct dir_entry *) dir_block->b_data;
	de->inode=inode->i_num;
	strcpy(de->name,".");
	de++;
	de->inode = dir->i_num;
	strcpy(de->name,"..");
	inode->i_nlinks = 2;
	dir_block->b_dirt = 1;
	brelse(dir_block);
	inode->i_mode = I_DIRECTORY | (mode & 0777 & ~current->umask);
	inode->i_dirt = 1;
	bh = add_entry(dir,basename,namelen,&de);
	if (!bh) {
		iput(dir);
		free_block(inode->i_dev,inode->i_zone[0]);
		inode->i_nlinks=0;
		iput(inode);
		return -ENOSPC;
	}
	de->inode = inode->i_num;
	bh->b_dirt = 1;
	dir->i_nlinks++;
	dir->i_dirt = 1;
	iput(dir);
	iput(inode);
	brelse(bh);
	return 0;
}

/*
 * routine to check that the specified directory is empty (for rmdir)
 */
static int empty_dir(struct m_inode * inode)
{
	int nr,block;
	int len;
	struct buffer_head * bh;
	struct dir_entry * de;

	len = inode->i_size / sizeof (struct dir_entry);
	if (len<2 || !inode->i_zone[0] ||
	    !(bh=bread(inode->i_dev,inode->i_zone[0]))) {
	    	printk("warning - bad directory on dev %04x\n",inode->i_dev);
		return 0;
	}
	de = (struct dir_entry *) bh->b_data;
	if (de[0].inode != inode->i_num || !de[1].inode || 
	    strcmp(".",de[0].name) || strcmp("..",de[1].name)) {
	    	printk("warning - bad directory on dev %04x\n",inode->i_dev);
		return 0;
	}
	nr = 2;
	de += 2;
	while (nr<len) {
		if ((void *) de >= (void *) (bh->b_data+BLOCK_SIZE)) {
			brelse(bh);
			block=bmap(inode,nr/DIR_ENTRIES_PER_BLOCK);
			if (!block) {
				nr += DIR_ENTRIES_PER_BLOCK;
				continue;
			}
			if (!(bh=bread(inode->i_dev,block)))
				return 0;
			de = (struct dir_entry *) bh->b_data;
		}
		if (de->inode) {
			brelse(bh);
			return 0;
		}
		de++;
		nr++;
	}
	brelse(bh);
	return 1;
}

int sys_rmdir(const char * name)
{
	const char * basename;
	int namelen;
	struct m_inode * dir, * inode;
	struct buffer_head * bh;
	struct dir_entry * de;

	if (!suser())
		return -EPERM;
	if (!(dir = dir_namei(name,&namelen,&basename)))
		return -ENOENT;
	if (!namelen) {
		iput(dir);
		return -ENOENT;
	}
	if (!permission(dir,MAY_WRITE)) {
		iput(dir);
		return -EPERM;
	}
	bh = find_entry(&dir,basename,namelen,&de);
	if (!bh) {
		iput(dir);
		return -ENOENT;
	}
	if (!(inode = iget(dir->i_dev, de->inode))) {
		iput(dir);
		brelse(bh);
		return -EPERM;
	}
	if ((dir->i_mode & S_ISVTX) && current->euid &&
	    inode->i_uid != current->euid) {
		iput(dir);
		iput(inode);
		brelse(bh);
		return -EPERM;
	}
	if (inode->i_dev != dir->i_dev || inode->i_count>1) {
		iput(dir);
		iput(inode);
		brelse(bh);
		return -EPERM;
	}
	if (inode == dir) {	/* we may not delete ".", but "../dir" is ok */
		iput(inode);
		iput(dir);
		brelse(bh);
		return -EPERM;
	}
	if (!S_ISDIR(inode->i_mode)) {
		iput(inode);
		iput(dir);
		brelse(bh);
		return -ENOTDIR;
	}
	if (!empty_dir(inode)) {
		iput(inode);
		iput(dir);
		brelse(bh);
		return -ENOTEMPTY;
	}
	if (inode->i_nlinks != 2)
		printk("empty directory has nlink!=2 (%d)",inode->i_nlinks);
	de->inode = 0;
	bh->b_dirt = 1;
	brelse(bh);
	inode->i_nlinks=0;
	inode->i_dirt=1;
	dir->i_nlinks--;
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	dir->i_dirt=1;
	iput(dir);
	iput(inode);
	return 0;
}

int sys_unlink(const char * name)
{
	const char * basename;
	int namelen;
	struct m_inode * dir, * inode;
	struct buffer_head * bh;
	struct dir_entry * de;

	if (!(dir = dir_namei(name,&namelen,&basename)))
		return -ENOENT;
	if (!namelen) {
		iput(dir);
		return -ENOENT;
	}
	if (!permission(dir,MAY_WRITE)) {
		iput(dir);
		return -EPERM;
	}
	bh = find_entry(&dir,basename,namelen,&de);
	if (!bh) {
		iput(dir);
		return -ENOENT;
	}
	if (!(inode = iget(dir->i_dev, de->inode))) {
		iput(dir);
		brelse(bh);
		return -ENOENT;
	}
	if ((dir->i_mode & S_ISVTX) && !suser() &&
	    current->euid != inode->i_uid &&
	    current->euid != dir->i_uid) {
		iput(dir);
		iput(inode);
		brelse(bh);
		return -EPERM;
	}
	if (S_ISDIR(inode->i_mode)) {
		iput(inode);
		iput(dir);
		brelse(bh);
		return -EPERM;
	}
	if (!inode->i_nlinks) {
		printk("Deleting nonexistent file (%04x:%d), %d\n",
			inode->i_dev,inode->i_num,inode->i_nlinks);
		inode->i_nlinks=1;
	}
	de->inode = 0;
	bh->b_dirt = 1;
	brelse(bh);
	inode->i_nlinks--;
	inode->i_dirt = 1;
	inode->i_ctime = CURRENT_TIME;
	iput(inode);
	iput(dir);
	return 0;
}

int sys_link(const char * oldname, const char * newname)
{
	struct dir_entry * de;
	struct m_inode * oldinode, * dir;
	struct buffer_head * bh;
	const char * basename;
	int namelen;

	oldinode=namei(oldname);
	if (!oldinode)
		return -ENOENT;
	if (S_ISDIR(oldinode->i_mode)) {
		iput(oldinode);
		return -EPERM;
	}
	dir = dir_namei(newname,&namelen,&basename);
	if (!dir) {
		iput(oldinode);
		return -EACCES;
	}
	if (!namelen) {
		iput(oldinode);
		iput(dir);
		return -EPERM;
	}
	if (dir->i_dev != oldinode->i_dev) {
		iput(dir);
		iput(oldinode);
		return -EXDEV;
	}
	if (!permission(dir,MAY_WRITE)) {
		iput(dir);
		iput(oldinode);
		return -EACCES;
	}
	bh = find_entry(&dir,basename,namelen,&de);
	if (bh) {
		brelse(bh);
		iput(dir);
		iput(oldinode);
		return -EEXIST;
	}
	bh = add_entry(dir,basename,namelen,&de);
	if (!bh) {
		iput(dir);
		iput(oldinode);
		return -ENOSPC;
	}
	de->inode = oldinode->i_num;
	bh->b_dirt = 1;
	brelse(bh);
	iput(dir);
	oldinode->i_nlinks++;
	oldinode->i_ctime = CURRENT_TIME;
	oldinode->i_dirt = 1;
	iput(oldinode);
	return 0;
}
