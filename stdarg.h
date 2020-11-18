#ifndef _STDARG_H
#define _STDARG_H

typedef char *va_list;

/* Amount of space required in an argument list for an arg of type TYPE.

10、根据代码详细分析，进程0如何根据调度第一次切换到进程1的？（P103-107）
答：
① 进程0通过fork函数创建进程1，使其处在就绪态。
② 进程0调用pause函数。pause函数通过int 0x80中断，映射到sys_pause函数，将自身设为可中断等待状态，调用schedule函数。
③ schedule函数分析到当前有必要进行进程调度，第一次遍历进程，只要地址指针不为为空，就要针对处理。第二次遍历所有进程，比较进程的状态和时间骗，找出处在就绪态且counter最大的进程，此时只有进程0和1，且进程0是可中断等待状态，只有进程1是就绪态，所以切换到进程1去执行。
 

11、为什么要设计缓冲区，有什么好处？
缓冲区的作用主要体现在两方面：
②	 形成所有块设备数据的统一集散地，操作系统的设计更方便，更灵活；
② 数据块复用，提高对块设备文件操作的运行效率。在计算机中，内存间的数据交换速度是内存与硬盘数据交换速度的2个量级，如果某个进程将硬盘数据读到缓冲区之后，其他进程刚好也需要读取这些数据，那么就可以直接从缓冲区中读取，比直接从硬盘读取快很多。如果缓冲区的数据能够被更多进程共享的话，计算机的整体效率就会大大提高。同样，写操作类似。
缓冲区是内存与外设（块设备，如硬盘等）进行数据交互的媒介。内存与外设最大的区别在 于：外设（如硬盘）的作用仅仅就是对数据信息以逻辑块的形式进行断电保存，并不参与运 算（因为 CPU 无法到硬盘上进行寻址）；而内存除了需要对数据进行保存以外，还要通过 与 CPU 和总线的配合，进行数据运算（有代码和数据之分）；缓冲区则介于两者之间，有 了缓冲区这个媒介以后，对外设而言，它仅需要考虑与缓冲区进行数据交互是否符合要求， 而不需要考虑内存中内核、进程如何使用这些数据；对内存的内核、进程而言，它也仅需要 考虑与缓冲区交互的条件是否成熟，而并不需要关心此时外设对缓冲区的交互情况。它们两 者的组织、管理和协调将由操作系统统一操作，这样就大大降低了数据处理的维护成本。

   TYPE may alternatively be an expression whose type is used.  */

#define __va_rounded_size(TYPE)  \
  (((sizeof (TYPE) + sizeof (int) - 1) / sizeof (int)) * sizeof (int))

#ifndef __sparc__
#define va_start(AP, LASTARG) 						\
 (AP = ((char *) &(LASTARG) + __va_rounded_size (LASTARG)))
#else
#define va_start(AP, LASTARG) 						\
 (__builtin_saveregs (),						\
  AP = ((char *) &(LASTARG) + __va_rounded_size (LASTARG)))
#endif

void va_end (va_list);		/* Defined in gnulib */
#define va_end(AP)

#define va_arg(AP, TYPE)						\
 (AP += __va_rounded_size (TYPE),					\
  *((TYPE *) (AP - __va_rounded_size (TYPE))))

#endif /* _STDARG_H */
