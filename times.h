#ifndef _TIMES_H
#define _TIMES_H

/**
 * 22. 在虚拟盘被设置为根设备之前，操作系统的根设备是软盘，请说明设置软盘为根设备的技术路线。
首先，将软盘的第一个山区设置为可引导扇区:
（代码路径： boot/bootsect.s） boot_flag: .word 0xAA55
在主 Makefile 文件中设置 ROOT_DEV=/dev/hd6。 并且在 bootsect.s 中的 508 和 509 处设置ROOT_DEV=0x306；在 tools/build 中根据 Makefile 中的 ROOT_DEV 设置 MAJOR_TOOT和 MINOR_ROOT，并将其填充在偏移量为 508 和 509 处：
(代码路径： Makefile) tools/build boot/bootsect boot/setup tools/system $(ROOT_DEV) > Image
随后被移至 0x90000+508(即 0x901FC)处，最终在 main.c 中设置为RIG_ROOT_DEV 并将其赋给 ROOT_DEV 变量：
(代码路径： init/main.c)
62 #define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)
113 ROOT_DEV = ORIG_ROOT_DEV;


23. Linux0.11 是怎么将根设备从软盘更换为虚拟盘，并加载了根文件系统？
rd_load 函数从软盘读取文件系统并将其复制到虚拟盘中并通过设置 ROOT_DEV 为 0x0101将根设备从软盘更换为虚拟盘，然后调用 mount_root 函数加载跟文件系统，过程如下：初始化 file_table 和 super_block，初始化 super_block 并读取根 i 节点，然后统计空闲逻辑块数及空闲 i 节点数：
(代码路径： kernel/blk_drv/ramdisk.c:rd_load) ROOT_DEV=0x0101;
主设备好是 1，代表内存，即将内存虚拟盘设置为根目录

 * 
 * 
*/

#include <sys/types.h>

struct tms {
	time_t tms_utime;
	time_t tms_stime;
	time_t tms_cutime;
	time_t tms_cstime;
};

extern time_t times(struct tms * tp);

#endif
