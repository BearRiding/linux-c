#ifndef _CONFIG_H
#define _CONFIG_H

/*
 * The root-device is no longer hard-coded. You can change the default
 * root-device by changing the line ROOT_DEV = XXX in boot/bootsect.s
 */

/**
 * define your keyboard here -
 * KBD_FINNISH for Finnish keyboards
 * KBD_US for US-type
 * KBD_GR for German keyboards
 * KBD_FR for Frech keyboard
 * 1.为什么开始启动计算机的时候，
 * 执行的是BIOS代码而不是操作系统自身的代码？
 * 计算机启动的时候，内存未初始化，
 * CPU不能直接从外设运行操作系统，所以必须将操作系统加载至内存中。
 * 而这个工作最开始的部分，BIOS 需要完成一些检测工作，
 * 和设置实模式下的中断向量表和服务程序，并将操作系统的引导扇区加载值0x7C00处，
 * 然后将跳转至0x7C00。这些就是由 bios 程序来实现的。所以计算机启动最开始执行的是 bios 代码。 

 */
/*#define KBD_US */
/*#define KBD_GR */
/*#define KBD_FR */
#define KBD_FINNISH

/*
 * Normally, Linux can get the drive parameters from the BIOS at
 * startup, but if this for some unfathomable reason fails, you'd
 * be left stranded. For this case, you can define HD_TYPE, which
 * contains all necessary info on your harddisk.
 *
 * The HD_TYPE macro should look like this:
 *
 * #define HD_TYPE { head, sect, cyl, wpcom, lzone, ctl}
 *
 * In case of two harddisks, the info should be sepatated by
 * commas:
 *
 * #define HD_TYPE { h,s,c,wpcom,lz,ctl },{ h,s,c,wpcom,lz,ctl }
 */
/*
 This is an example, two drives, first is type 2, second is type 3:

#define HD_TYPE { 4,17,615,300,615,8 }, { 6,17,615,300,615,0 }

 NOTE: ctl is 0 for all drives with heads<=8, and ctl=8 for drives
 with more than 8 heads.

 If you want the BIOS to tell what kind of drive you have, just
 leave HD_TYPE undefined. This is the normal thing to do.
*/

#endif
