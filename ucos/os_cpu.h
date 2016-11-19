/*
*********************************************************************************************************
*                                               uC/OS-II
*                                         The Real-Time Kernel
*
*
*                                (c) Copyright 2006, Micrium, Weston, FL
*                                          All Rights Reserved
*
*                                           ARM Cortex-M3 Port
*
* File      : OS_CPU.H
* Version   : V2.86
* By        : Jean J. Labrosse
*             Brian Nagel
*
* For       : ARMv7M Cortex-M3
* Mode      : Thumb2
* Toolchain : IAR EWARM
*********************************************************************************************************
*/

#ifndef  __OS_CPU_H__
#define  __OS_CPU_H__


#ifdef   OS_CPU_GLOBALS
#define  OS_CPU_EXT
#else
#define  OS_CPU_EXT  extern
#endif

/*
*********************************************************************************************************
*                                              DATA TYPES
*                                         (Compiler Specific)
*********************************************************************************************************
*/

typedef unsigned char  BOOLEAN;
typedef unsigned char  INT8U;                    /* Unsigned  8 bit quantity                           */
typedef signed   char  INT8S;                    /* Signed    8 bit quantity                           */
typedef unsigned short INT16U;                   /* Unsigned 16 bit quantity                           */
typedef signed   short INT16S;                   /* Signed   16 bit quantity                           */
typedef unsigned int   INT32U;                   /* Unsigned 32 bit quantity                           */
typedef signed   int   INT32S;                   /* Signed   32 bit quantity                           */
typedef float          FP32;                     /* Single precision floating point                    */
typedef double         FP64;                     /* Double precision floating point                    */

typedef unsigned int   OS_STK;                   /* Each stack entry is 32-bit wide                    */
typedef unsigned int   OS_CPU_SR;                /* Define size of CPU status register (PSR = 32 bits) */

/*
*********************************************************************************************************
*                                              Cortex-M1
*                                      Critical Section Management
*
* Method #1:  Disable/Enable interrupts using simple instructions.  After critical section, interrupts
*             will be enabled even if they were disabled before entering the critical section.
*             NOT IMPLEMENTED
*             使用微处理器指令开关中断，譬如MCS-51中：
*             #define OS_ENTER_CRITICAL()       EA=0;
*             #define OS_EXIT_CRITICAL()        EA=1;
*             而在Cortex-M3中:
*             #define OS_ENTER_CRITICAL()       {__ASM volatile ("cpsid i");}
*             #define OS_EXIT_CRITICAL()        {__ASM volatile ("cpsie i");}
*             优点：简单直接通用
*             缺点：如果在进入临界区之前中断就是关闭的，那么在退出临界区时会将中断打开。
*
* Method #2:  Disable/Enable interrupts by preserving the state of interrupts.  In other words, if
*             interrupts were disabled before entering the critical section, they will be disabled when
*             leaving the critical section.
*             NOT IMPLEMENTED
*             关中断：先利用堆栈保存中断状态，然后关中断            PUSH PSW;   DI;
*             开中断：将中断开关状态从堆栈弹出，恢复中断原始状态    POP  PSW;
*             优点：保护了中断原始状态，解决了方法#1中的缺点，进临界区之前中断是开的或是关的，退出临界区时中断还是开的或是关的
*             缺点：必须使用汇编，如果某些编译器对嵌入汇编代码优化的不好，会导致严重错误。
*
* Method #3:  Disable/Enable interrupts by preserving the state of interrupts.  Generally speaking you
*             would store the state of the interrupt disable flag in the local variable 'cpu_sr' and then
*             disable interrupts.  'cpu_sr' is allocated in all of uC/OS-II's functions that need to
*             disable interrupts.  You would restore the interrupt disable state by copying back 'cpu_sr'
*             into the CPU's status register.
*             为了解决方法2中的缺点，对方法2进行改进，将PSW存储在一个局部变量cpu_sr中。
*             关中断：通过OS_CPU_SR_Save()函数获取PSW的值，存储在cpu_sr中
*             开中断：将cpu_sr中存储的数值恢复到PSW中，即返回中断开关状态。
*             这样不使用堆栈，绕过了汇编代码，也可以实现对中断装态的保存。*^_^*
*********************************************************************************************************
*/

#define  OS_CRITICAL_METHOD   3     //选择采用哪一种开关中断的宏定义，一共有三种，对应着上面的Method #1~#3

#if OS_CRITICAL_METHOD == 3
#define  OS_ENTER_CRITICAL()  {cpu_sr = OS_CPU_SR_Save();}
#define  OS_EXIT_CRITICAL()   {OS_CPU_SR_Restore(cpu_sr);}
#endif

/*
*********************************************************************************************************
*                                        Cortex-M3 Miscellaneous
*********************************************************************************************************
*/

#define  OS_STK_GROWTH        1                   /* Stack grows from HIGH to LOW memory on ARM        */

#define  OS_TASK_SW()         OSCtxSw()           //用宏定义来替代汇编函数，因为C语言不能直接处理寄存器

/*
*********************************************************************************************************
*                                              PROTOTYPES
*********************************************************************************************************
*/

#if OS_CRITICAL_METHOD == 3                       /* See OS_CPU_A.ASM                                  */
OS_CPU_SR  OS_CPU_SR_Save(void);
void       OS_CPU_SR_Restore(OS_CPU_SR cpu_sr);
#endif

void       OSCtxSw(void);
void       OSIntCtxSw(void);
void       OSStartHighRdy(void);

void       OS_CPU_PendSVHandler(void);

                                                  /* See OS_CPU_C.C                                    */
//void       OS_CPU_SysTickHandler(void);
//void       OS_CPU_SysTickInit(void);

                                                  /* See BSP.C                                         */
//INT32U     OS_CPU_SysTickClkFreq(void);
#endif
