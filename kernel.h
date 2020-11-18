/*
 * 'kernel.h' contains some often-used function prototypes etc
 * 
 * 13，cpl、rpl、dpl分别是什么意思？记录在什么位置？
描述符特权级（DPL）域——（段描述符的第二个双字的第 13 和第 14 位）确定段的特权级。
请求特权级（RPL）域——（段选择子的第 0 和第 1 位）确定段选择子的请求特权级。
当前特权级（CPL）域——（CS 段寄存器的第 0 和第 1 位）指明当前执行程序/例程的特权级。
术语“当前特权级（CPL）”就是指这个域的设置。

 */
void verify_area(void * addr,int count);
volatile void panic(const char * str);
int printf(const char * fmt, ...);
int printk(const char * fmt, ...);
int tty_write(unsigned ch,char * buf,int count);
void * malloc(unsigned int size);
void free_s(void * obj, int size);

#define free(x) free_s((x), 0)

/*
 * This is defined as a macro, but at some point this might become a
 * real subroutine that sets a flag if it returns true (to do
 * BSD-style accounting where the process is flagged if it uses root
 * privs).  The implication of this is that you should do normal
 * permissions checks first, and check suser() last.
 */
#define suser() (current->euid == 0)

