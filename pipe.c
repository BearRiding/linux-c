/*
 *  linux/fs/pipe.c
 *
 *  (C) 1991  Linus Torvalds
 * 
 * 21、详细分析进程调度的全过程。考虑所有可能（signal、alarm除外）
首先在 task 数据（进程槽）中，从后往前进行遍历，寻找进程槽中，进程状态为“就緒态”且时间片最大的进程作为下一个要执行的进程。通过调用 switch_to()函数跳转到指定进程。在此过程中，如果发现存在状态为“就緒态”的进程，但这些进程都没有时间片了，则会从后往前遍历进程槽为所有进程重新分配时间片（不仅仅是“就緒态”的进程）。然后再重新执行以上步骤，寻找进程槽中，进程状态为“就緒态”且时间片最大的进程作为下一个要执行的进程。如果在遍历的过程中，发现没有进程处于“就绪态”，则会调用 switch_to()函数跳转到进程 0。
在 switch_to 函数中，如果要跳转的目标进程就是当前进程，则不发生跳转。否则，保存当前进程信息，长跳转到目标进程。

22、wait_on_buffer函数中为什么不用if（）而是用while（）？
因为可能存在一种情况是，很多进程都在等待一个缓冲块。在缓冲块同步完毕，唤醒各等待进程到轮转到某一进程的过程中，很有可能此时的缓冲块又被其它进程所占用，并被加上了锁。此时如果用 if()，则此进程会从之前被挂起的地方继续执行，不会再判断是否缓冲块已被占用而直接使用，就会出现错误；而如果用 while()，则此进程会再次确认缓冲块是否已被占用，在确认未被占用后，才会使用，这样就不会发生之前那样的错误。

 */

#include <signal.h>

#include <linux/sched.h>
#include <linux/mm.h>	/* for get_free_page */
#include <asm/segment.h>

int read_pipe(struct m_inode * inode, char * buf, int count)
{
	int chars, size, read = 0;

	while (count>0) {
		while (!(size=PIPE_SIZE(*inode))) {
			wake_up(&inode->i_wait);
			if (inode->i_count != 2) /* are there any writers? */
				return read;
			sleep_on(&inode->i_wait);
		}
		chars = PAGE_SIZE-PIPE_TAIL(*inode);
		if (chars > count)
			chars = count;
		if (chars > size)
			chars = size;
		count -= chars;
		read += chars;
		size = PIPE_TAIL(*inode);
		PIPE_TAIL(*inode) += chars;
		PIPE_TAIL(*inode) &= (PAGE_SIZE-1);
		while (chars-->0)
			put_fs_byte(((char *)inode->i_size)[size++],buf++);
	}
	wake_up(&inode->i_wait);
	return read;
}
	
int write_pipe(struct m_inode * inode, char * buf, int count)
{
	int chars, size, written = 0;

	while (count>0) {
		while (!(size=(PAGE_SIZE-1)-PIPE_SIZE(*inode))) {
			wake_up(&inode->i_wait);
			if (inode->i_count != 2) { /* no readers */
				current->signal |= (1<<(SIGPIPE-1));
				return written?written:-1;
			}
			sleep_on(&inode->i_wait);
		}
		chars = PAGE_SIZE-PIPE_HEAD(*inode);
		if (chars > count)
			chars = count;
		if (chars > size)
			chars = size;
		count -= chars;
		written += chars;
		size = PIPE_HEAD(*inode);
		PIPE_HEAD(*inode) += chars;
		PIPE_HEAD(*inode) &= (PAGE_SIZE-1);
		while (chars-->0)
			((char *)inode->i_size)[size++]=get_fs_byte(buf++);
	}
	wake_up(&inode->i_wait);
	return written;
}

int sys_pipe(unsigned long * fildes)
{
	struct m_inode * inode;
	struct file * f[2];
	int fd[2];
	int i,j;

	j=0;
	for(i=0;j<2 && i<NR_FILE;i++)
		if (!file_table[i].f_count)
			(f[j++]=i+file_table)->f_count++;
	if (j==1)
		f[0]->f_count=0;
	if (j<2)
		return -1;
	j=0;
	for(i=0;j<2 && i<NR_OPEN;i++)
		if (!current->filp[i]) {
			current->filp[ fd[j]=i ] = f[j];
			j++;
		}
	if (j==1)
		current->filp[fd[0]]=NULL;
	if (j<2) {
		f[0]->f_count=f[1]->f_count=0;
		return -1;
	}
	if (!(inode=get_pipe_inode())) {
		current->filp[fd[0]] =
			current->filp[fd[1]] = NULL;
		f[0]->f_count = f[1]->f_count = 0;
		return -1;
	}
	f[0]->f_inode = f[1]->f_inode = inode;
	f[0]->f_pos = f[1]->f_pos = 0;
	f[0]->f_mode = 1;		/* read */
	f[1]->f_mode = 2;		/* write */
	put_fs_long(fd[0],0+fildes);
	put_fs_long(fd[1],1+fildes);
	return 0;
}
