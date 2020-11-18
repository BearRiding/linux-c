#ifndef _ERRNO_H
#define _ERRNO_H

/**
 * ok, as I hadn't got any other source of information about
 * possible error numbers, I was forced to use the same numbers
 * as minix.
 * Hopefully these are posix or something. I wouldn't know (and posix
 * isn't telling me - they want $$$ for their f***ing standard).
 *
 * We don't use the _SIGN cludge of minix, so kernel returns must
 * see to the sign by themselves.
 *
 * NOTE! Remember to change strerror() if you change this file!
 * 3.为什么BIOS把bootsect加载到0x07c00，而不是0x00000？加载后又马上挪到0x90000处，
 * 是何道理？为什么不一次加载到位？
 * 1）因为BIOS将从 0x00000 开始的 1KB 字节构建了了中断向量表，
 * 接着的 256KB 字节内存空间构建了 BIOS 数据区，
 * 所以不能把 bootsect 加载到 0x00000. 0X07c00 是 BIOS 设置的 内存地址，
 * 不是 bootsect 能够决定的。
 * 2）首先，在启动扇区中有一些数据，将会被内核利用到。
 * 其次，依据系统对内存的规划，内核终会占用0x0000起始的空间，
 * 因此0x7c00可能会被覆盖。将该扇区挪到 0x90000，在setup.s中，
 * 获取一些硬件数据保存在 0x90000~0x901ff处，可以 对一些后面内核将要利用的数据，集中保存和管理。

 */

extern int errno;

#define ERROR		99
#define EPERM		 1
#define ENOENT		 2
#define ESRCH		 3
#define EINTR		 4
#define EIO		 5
#define ENXIO		 6
#define E2BIG		 7
#define ENOEXEC		 8
#define EBADF		 9
#define ECHILD		10
#define EAGAIN		11
#define ENOMEM		12
#define EACCES		13
#define EFAULT		14
#define ENOTBLK		15
#define EBUSY		16
#define EEXIST		17
#define EXDEV		18
#define ENODEV		19
#define ENOTDIR		20
#define EISDIR		21
#define EINVAL		22
#define ENFILE		23
#define EMFILE		24
#define ENOTTY		25
#define ETXTBSY		26
#define EFBIG		27
#define ENOSPC		28
#define ESPIPE		29
#define EROFS		30
#define EMLINK		31
#define EPIPE		32
#define EDOM		33
#define ERANGE		34
#define EDEADLK		35
#define ENAMETOOLONG	36
#define ENOLCK		37
#define ENOSYS		38
#define ENOTEMPTY	39

#endif
