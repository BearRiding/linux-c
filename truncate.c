/*
 *  linux/fs/truncate.c
 *
 *  (C) 1991  Linus Torvalds
 * 
 * 26、电梯算法完成后为什么没有执行do_hd_request函数？
不用do_hd_request是因为在读写的中断处理函数完成了end_request之后，还会主动调用do_hd_request，end_request会主动将CURRENT指向下一个请求，然后do_hd_request自然就可以继续处理下一个请求，完成对这个设备上所有请求的响应。



27. 用图表示下面的几种情况，并从代码中找到证据：
A当进程获得第一个缓冲块的时候，hash表的状态
B经过一段时间的运行。已经有2000多个buffer_head挂到hash_table上时，hash表（包括所有的buffer_head）的整体运行状态。
C经过一段时间的运行，有的缓冲块已经没有进程使用了（空闲），这样的空闲缓冲块是否会从hash_table上脱钩？
D经过一段时间的运行，所有的buffer_head都挂到hash_table上了，这时，又有进程申请空闲缓冲块，将会发生什么？

A
getblk(int dev, int block) à get_hash_table(dev,block) -> find_buffer(dev,block) -> hash(dev, block)
哈希策略为：
              #define _hashfn(dev,block)(((unsigned)(dev block))%NR_HASH)
              #define hash(dev,block) hash_table[_hashfn(dev, block)]

此时，dev为0x300，block为0，NR_HASH为307，哈希结果为154，将此块插入哈希表中次位置后

B
//代码路径 ：fs/buffer.c:   
…
static inline void insert_into_queues(struct buffer_head * bh) {
/*put at end of free list */   
bh->b_next_free= free_list;   
bh->b_prev_free= free_list->b_prev_free;   
free_list->b_prev_free->b_next_free= bh;   
free_list->b_prev_free= bh;
/*put the buffer in new hash-queue if it has a device */   
bh->b_prev= NULL;   
bh->b_next= NULL;   
if (!bh->b_dev)        
return;  

bh->b_next= hash(bh->b_dev,bh->b_blocknr);   
hash(bh->b_dev,bh->b_blocknr)= bh;   
bh->b_next->b_prev= bh
       }

C
不会脱钩，会调用brelse()函数，其中if(!(buf->b_count--))，计数器减一。没有对该缓冲块执行remove操作。由于硬盘读写开销一般比内存大几个数量级，因此该空闲缓冲块若是能够再次被访问到，对提升性能是有益的。

D
进程顺着freelist找到没被占用的，未被上锁的干净的缓冲块后，将其引用计数置为1，然后从hash队列和空闲块链表中移除该bh，然后根据此新的设备号和块号重新插入空闲表和哈西队列新位置处，最终返回缓冲头指针。
Bh->b_count=1;
Bh->b_dirt=0;
Bh->b_uptodate=0;
Remove_from_queues(bh);
Bh->b_dev=dev;
Bh->b_blocknr=block;
Insert_into_queues(bh);



 */

#include <linux/sched.h>

#include <sys/stat.h>

static void free_ind(int dev,int block)
{
	struct buffer_head * bh;
	unsigned short * p;
	int i;

	if (!block)
		return;
	if (bh=bread(dev,block)) {
		p = (unsigned short *) bh->b_data;
		for (i=0;i<512;i++,p++)
			if (*p)
				free_block(dev,*p);
		brelse(bh);
	}
	free_block(dev,block);
}

static void free_dind(int dev,int block)
{
	struct buffer_head * bh;
	unsigned short * p;
	int i;

	if (!block)
		return;
	if (bh=bread(dev,block)) {
		p = (unsigned short *) bh->b_data;
		for (i=0;i<512;i++,p++)
			if (*p)
				free_ind(dev,*p);
		brelse(bh);
	}
	free_block(dev,block);
}

void truncate(struct m_inode * inode)
{
	int i;

	if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
		return;
	for (i=0;i<7;i++)
		if (inode->i_zone[i]) {
			free_block(inode->i_dev,inode->i_zone[i]);
			inode->i_zone[i]=0;
		}
	free_ind(inode->i_dev,inode->i_zone[7]);
	free_dind(inode->i_dev,inode->i_zone[8]);
	inode->i_zone[7] = inode->i_zone[8] = 0;
	inode->i_size = 0;
	inode->i_dirt = 1;
	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
}

