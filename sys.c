/*
 *  linux/kernel/sys.c
 * 
 * 16.根据代码详细说明 copy_process 函数的所有参数是如何形成的？
long eip, long cs, long eflags, long esp, long ss；这五个参数是中断使 CPU 自动压栈的。
long ebx, long ecx, long edx, long fs, long es, long ds 为__system_call 压进栈的参数。
long none 为__system_call 调用__sys_fork 压进栈 EIP 的值。
Int nr, long ebp, long edi, long esi, long gs,为__system_call 压进栈的值。



17.进程 0 创建进程 1 时调用 copy_process 函数，在其中直接、间接调用了两次 get_free_page函数，在物理内存中获得了两个页，分别用作什么？是怎么设置的？给出代码证据。（P89 91 92P97-98 ）
第一次调用get_free_page函数申请的空闲页面用于进程1 的task_struct及内核栈。首先将申请到的页面清0，然后复制进程0的task_struct，再针对进程1作个性化设置，其中esp0 的设置，意味着设置该页末尾为进程 1 的堆栈的起始地址。代码见P90 及 P92。
sched.c
struct task_struct *current = &(init_task.task);
fork.c
p = (struct task_struct *) get_free_page();
…
*p = *current;
p->tss.esp0 = PAGE_SIZE + (long) p;
其中*current 即为进程 0 的 task 结构，在 copy_process 中，先复制进程 0 的 task_struct，然后再对其中的值进行修改。 esp0 的设置，意味着设置该页末尾为进程 1 的堆栈的起始地址。
第二次调用get_free_page函数申请的空闲页面用于进程1的页表。在创建进程1执行copy_process中，执行copy_mem(nr,p)时，内核为进程1拷贝了进程 0的页表（160 项），同时修改了页表项的属性为只读。代码见P98。。
copy_mem
…
if (copy_page_tables(old_data_base,new_data_base,data_limit)){
…
其中， copy_page_tables 内部
…
from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
if (!(to_page_table = (unsigned long *) get_free_page()))
…
for ( ; nr-- > 0 ; from_page_table++,to_page_table++) {
… }
获取了新的页，且从 from_page_table 将页表值拷贝到 to_page_table 处。

 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <sys/times.h>
#include <sys/utsname.h>

int sys_ftime()
{
	return -ENOSYS;
}

int sys_break()
{
	return -ENOSYS;
}

int sys_ptrace()
{
	return -ENOSYS;
}

int sys_stty()
{
	return -ENOSYS;
}

int sys_gtty()
{
	return -ENOSYS;
}

int sys_rename()
{
	return -ENOSYS;
}

int sys_prof()
{
	return -ENOSYS;
}

int sys_setregid(int rgid, int egid)
{
	if (rgid>0) {
		if ((current->gid == rgid) || 
		    suser())
			current->gid = rgid;
		else
			return(-EPERM);
	}
	if (egid>0) {
		if ((current->gid == egid) ||
		    (current->egid == egid) ||
		    (current->sgid == egid) ||
		    suser())
			current->egid = egid;
		else
			return(-EPERM);
	}
	return 0;
}

int sys_setgid(int gid)
{
	return(sys_setregid(gid, gid));
}

int sys_acct()
{
	return -ENOSYS;
}

int sys_phys()
{
	return -ENOSYS;
}

int sys_lock()
{
	return -ENOSYS;
}

int sys_mpx()
{
	return -ENOSYS;
}

int sys_ulimit()
{
	return -ENOSYS;
}

int sys_time(long * tloc)
{
	int i;

	i = CURRENT_TIME;
	if (tloc) {
		verify_area(tloc,4);
		put_fs_long(i,(unsigned long *)tloc);
	}
	return i;
}

/*
 * Unprivileged users may change the real user id to the effective uid
 * or vice versa.
 */
int sys_setreuid(int ruid, int euid)
{
	int old_ruid = current->uid;
	
	if (ruid>0) {
		if ((current->euid==ruid) ||
                    (old_ruid == ruid) ||
		    suser())
			current->uid = ruid;
		else
			return(-EPERM);
	}
	if (euid>0) {
		if ((old_ruid == euid) ||
                    (current->euid == euid) ||
		    suser())
			current->euid = euid;
		else {
			current->uid = old_ruid;
			return(-EPERM);
		}
	}
	return 0;
}

int sys_setuid(int uid)
{
	return(sys_setreuid(uid, uid));
}

int sys_stime(long * tptr)
{
	if (!suser())
		return -EPERM;
	startup_time = get_fs_long((unsigned long *)tptr) - jiffies/HZ;
	return 0;
}

int sys_times(struct tms * tbuf)
{
	if (tbuf) {
		verify_area(tbuf,sizeof *tbuf);
		put_fs_long(current->utime,(unsigned long *)&tbuf->tms_utime);
		put_fs_long(current->stime,(unsigned long *)&tbuf->tms_stime);
		put_fs_long(current->cutime,(unsigned long *)&tbuf->tms_cutime);
		put_fs_long(current->cstime,(unsigned long *)&tbuf->tms_cstime);
	}
	return jiffies;
}

int sys_brk(unsigned long end_data_seg)
{
	if (end_data_seg >= current->end_code &&
	    end_data_seg < current->start_stack - 16384)
		current->brk = end_data_seg;
	return current->brk;
}

/*
 * This needs some heave checking ...
 * I just haven't get the stomach for it. I also don't fully
 * understand sessions/pgrp etc. Let somebody who does explain it.
 */
int sys_setpgid(int pid, int pgid)
{
	int i;

	if (!pid)
		pid = current->pid;
	if (!pgid)
		pgid = current->pid;
	for (i=0 ; i<NR_TASKS ; i++)
		if (task[i] && task[i]->pid==pid) {
			if (task[i]->leader)
				return -EPERM;
			if (task[i]->session != current->session)
				return -EPERM;
			task[i]->pgrp = pgid;
			return 0;
		}
	return -ESRCH;
}

int sys_getpgrp(void)
{
	return current->pgrp;
}

int sys_setsid(void)
{
	if (current->leader && !suser())
		return -EPERM;
	current->leader = 1;
	current->session = current->pgrp = current->pid;
	current->tty = -1;
	return current->pgrp;
}

int sys_uname(struct utsname * name)
{
	static struct utsname thisname = {
		"linux .0","nodename","release ","version ","machine "
	};
	int i;

	if (!name) return -ERROR;
	verify_area(name,sizeof *name);
	for(i=0;i<sizeof *name;i++)
		put_fs_byte(((char *) &thisname)[i],i+(char *) name);
	return 0;
}

int sys_umask(int mask)
{
	int old = current->umask;

	current->umask = mask & 0777;
	return (old);
}
