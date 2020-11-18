/**
 * This file contains some defines for the AT-hd-controller.
 * Various sources. Check out some definitions (see comments with
 * a ques).
 * 9.Linux 是用C语言写的，为什么没有从 main还是开始，而是先运行 3 个汇编程序，道理何在？
 * main 函数运行在 32 位的保护模式下，但系统启动时默认为 16 位的实模式， 
 * 开机时 的 16 位实 模式与 main 函数执行需要的 32 位保护模式之间有很大的差距，
 * 这个差距需 要由 3 个汇编程序来 填补。其中 bootsect 负责加载， 
 * setup 与 head 则负责获取硬件参 数，准备 idt,gdt,开启 A20， PE,PG， 
 * 废弃旧的 16 位中断响应机制，建立新的 32 为 IDT，设置分页机制等。
 * 这些工作做完后，计算机处在了32位的保护模式状态了，调用main的条件 就算准备完毕。

 */
#ifndef _HDREG_H
#define _HDREG_H

/* Hd controller regs. Ref: IBM AT Bios-listing */
#define HD_DATA		0x1f0	/* _CTL when writing */
#define HD_ERROR	0x1f1	/* see err-bits */
#define HD_NSECTOR	0x1f2	/* nr of sectors to read/write */
#define HD_SECTOR	0x1f3	/* starting sector */
#define HD_LCYL		0x1f4	/* starting cylinder */
#define HD_HCYL		0x1f5	/* high byte of starting cyl */
#define HD_CURRENT	0x1f6	/* 101dhhhh , d=drive, hhhh=head */
#define HD_STATUS	0x1f7	/* see status-bits */
#define HD_PRECOMP HD_ERROR	/* same io address, read=error, write=precomp */
#define HD_COMMAND HD_STATUS	/* same io address, read=status, write=cmd */

#define HD_CMD		0x3f6

/* Bits of HD_STATUS */
#define ERR_STAT	0x01
#define INDEX_STAT	0x02
#define ECC_STAT	0x04	/* Corrected error */
#define DRQ_STAT	0x08
#define SEEK_STAT	0x10
#define WRERR_STAT	0x20
#define READY_STAT	0x40
#define BUSY_STAT	0x80

/* Values for HD_COMMAND */
#define WIN_RESTORE		0x10
#define WIN_READ		0x20
#define WIN_WRITE		0x30
#define WIN_VERIFY		0x40
#define WIN_FORMAT		0x50
#define WIN_INIT		0x60
#define WIN_SEEK 		0x70
#define WIN_DIAGNOSE		0x90
#define WIN_SPECIFY		0x91

/* Bits for HD_ERROR */
#define MARK_ERR	0x01	/* Bad address mark ? */
#define TRK0_ERR	0x02	/* couldn't find track 0 */
#define ABRT_ERR	0x04	/* ? */
#define ID_ERR		0x10	/* ? */
#define ECC_ERR		0x40	/* ? */
#define	BBD_ERR		0x80	/* ? */

struct partition {
	unsigned char boot_ind;		/* 0x80 - active (unused) */
	unsigned char head;		/* ? */
	unsigned char sector;		/* ? */
	unsigned char cyl;		/* ? */
	unsigned char sys_ind;		/* ? */
	unsigned char end_head;		/* ? */
	unsigned char end_sector;	/* ? */
	unsigned char end_cyl;		/* ? */
	unsigned int start_sect;	/* starting sector counting from 0 */
	unsigned int nr_sects;		/* nr of sectors in partition */
};

#endif
