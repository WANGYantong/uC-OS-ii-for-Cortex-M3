;********************************************************************************************************
;                                               uC/OS-II
;                                         The Real-Time Kernel
;
;                               (c) Copyright 1992-2006, Micrium, Weston, FL
;                                          All Rights Reserved
;
;                                           ARM Cortex-M3 Port
;
; File      : OS_CPU_A.ASM
; Version   : V2.86
; By        : Jean J. Labrosse
;             Brian Nagel
;
; For       : ARMv7M Cortex-M3
; Mode      : Thumb2
; Toolchain : IAR EWARM
;********************************************************************************************************

;********************************************************************************************************
;                                           PUBLIC FUNCTIONS
;********************************************************************************************************

    EXTERN  OSRunning                                           ; External references
    EXTERN  OSPrioCur
    EXTERN  OSPrioHighRdy
    EXTERN  OSTCBCur
    EXTERN  OSTCBHighRdy
    EXTERN  OSIntNesting
    EXTERN  OSIntExit
    EXTERN  OSTaskSwHook


    EXPORT  OS_CPU_SR_Save                                      ; Functions declared in this file
    EXPORT  OS_CPU_SR_Restore
    EXPORT  OSStartHighRdy
    EXPORT  OSCtxSw
    EXPORT  OSIntCtxSw
    EXPORT  OS_CPU_PendSVHandler

;********************************************************************************************************
;                                                EQUATES
;********************************************************************************************************

NVIC_INT_CTRL   EQU     0xE000ED04                              ; Interrupt control state register.
NVIC_SYSPRI14   EQU     0xE000ED22                              ; System priority register (priority 14).
NVIC_PENDSV_PRI EQU           0xFF                              ; PendSV priority value (lowest).
NVIC_PENDSVSET  EQU     0x10000000                              ; Value to trigger PendSV exception.

;********************************************************************************************************
;                                      CODE GENERATION DIRECTIVES
;********************************************************************************************************

        AREA |.text | ,CODE,READONLY,ALIGN=2
		THUMB
		REQUIRE8
		PRESERVE8

;********************************************************************************************************
;                                   CRITICAL SECTION METHOD 3 FUNCTIONS
;
; Description: Disable/Enable interrupts by preserving the state of interrupts.  Generally speaking you
;              would store the state of the interrupt disable flag in the local variable 'cpu_sr' and then
;              disable interrupts.  'cpu_sr' is allocated in all of uC/OS-II's functions that need to
;              disable interrupts.  You would restore the interrupt disable state by copying back 'cpu_sr'
;              into the CPU's status register.
;
; Prototypes :     OS_CPU_SR  OS_CPU_SR_Save(void);
;                  void       OS_CPU_SR_Restore(OS_CPU_SR cpu_sr);
;
;
; Note(s)    : 1) These functions are used in general like this:
;
;                 void Task (void *p_arg)
;                 {
;                 #if OS_CRITICAL_METHOD == 3          /* Allocate storage for CPU status register */
;                     OS_CPU_SR  cpu_sr;
;                 #endif
;
;                          :
;                          :
;                     OS_ENTER_CRITICAL();             /* cpu_sr = OS_CPU_SaveSR();                */
;                          :
;                          :
;                     OS_EXIT_CRITICAL();              /* OS_CPU_RestoreSR(cpu_sr);                */
;                          :
;                          :
;                 }
;********************************************************************************************************

OS_CPU_SR_Save
    MRS     R0, PRIMASK                                         ; Set prio int mask to mask all (except faults)
    CPSID   I
    BX      LR

OS_CPU_SR_Restore
    MSR     PRIMASK, R0
    BX      LR

;********************************************************************************************************
;                                          START MULTITASKING
;                                       void OSStartHighRdy(void)
;
; Note(s) : 1) This function triggers a PendSV exception (essentially, causes a context switch) to cause
;              the first task to start.
;
;           2) OSStartHighRdy() MUST:
;              a) Setup PendSV exception priority to lowest;
;              b) Set initial PSP to 0, to tell context switcher this is first run;
;              c) Set OSRunning to TRUE;
;              d) Trigger PendSV exception;
;              e) Enable interrupts (tasks will run with interrupts enabled).
;********************************************************************************************************

OSStartHighRdy
    LDR     R0, =NVIC_SYSPRI14                                  ; 系统优先级寄存器14地址存入R0.Set the PendSV exception priority
    LDR     R1, =NVIC_PENDSV_PRI				; PendSV优先级地址存入R1
    STRB    R1, [R0]						; PendSV优先级存入优先级寄存器14(PendSV在启动文件的中断向量表中出了堆栈指针外排序第14)

    MOVS    R0, #0                                              ; Set the PSP to 0 for initial context switch call
    MSR     PSP, R0						; PSP = 0

    LDR     R0, =OSRunning                                      ; OSRunning = TRUE
    MOVS    R1, #1
    STRB    R1, [R0]

    LDR     R0, =NVIC_INT_CTRL                                  ; Trigger the PendSV exception (causes context switch)
    LDR     R1, =NVIC_PENDSVSET
    STR     R1, [R0]						;触发PendSV中断,具体参见146~148行注释

    CPSIE   I                                                   ; 开中断Enable interrupts at processor level

OSStartHang
    B       OSStartHang                                         ; Should never get here


;********************************************************************************************************
;                               PERFORM A CONTEXT SWITCH (From task level)
;                                           void OSCtxSw(void)
;
; Note(s) : 1) OSCtxSw() is called when OS wants to perform a task context switch.  This function
;              triggers the PendSV exception which is where the real work is done.
;********************************************************************************************************
; Trigger the PendSV exception (causes context switch)
OSCtxSw
    LDR     R0, =NVIC_INT_CTRL                       ;R0中存储中断控制寄存器的地址
    LDR     R1, =NVIC_PENDSVSET                      ;R1中存储触发PendSV的数值
    STR     R1, [R0]                                 ;将数值装入中断控制寄存器触发PendSV中断
    BX      LR                                       ;返回

;********************************************************************************************************
;                             PERFORM A CONTEXT SWITCH (From interrupt level)
;                                         void OSIntCtxSw(void)
;
; Notes:    1) OSIntCtxSw() is called by OSIntExit() when it determines a context switch is needed as
;              the result of an interrupt.  This function simply triggers a PendSV exception which will
;              be handled when there are no more interrupts active and interrupts are enabled.
;********************************************************************************************************

OSIntCtxSw
    LDR     R0, =NVIC_INT_CTRL                                  ; Trigger the PendSV exception (causes context switch)
    LDR     R1, =NVIC_PENDSVSET
    STR     R1, [R0]
    BX      LR

;********************************************************************************************************
;                                         HANDLE PendSV EXCEPTION
;                                     void OS_CPU_PendSVHandler(void)
;
; Note(s) : 1) PendSV is used to cause a context switch.  This is a recommended method for performing
;              context switches with Cortex-M3.  This is because the Cortex-M3 auto-saves half of the
;              processor context on any exception, and restores same on return from exception.  So only
;              saving of R4-R11 is required and fixing up the stack pointers.  Using the PendSV exception
;              this way means that context saving and restoring is identical whether it is initiated from
;              a thread or occurs due to an interrupt or exception.
;
;           2) Pseudo-code is:
;              a) Get the process SP, if 0 then skip (goto d) the saving part (first context switch);
;              b) Save remaining regs r4-r11 on process stack;
;              c) Save the process SP in its TCB, OSTCBCur->OSTCBStkPtr = SP;
;              d) Call OSTaskSwHook();
;              e) Get current high priority, OSPrioCur = OSPrioHighRdy;
;              f) Get current ready thread TCB, OSTCBCur = OSTCBHighRdy;
;              g) Get new process SP from TCB, SP = OSTCBHighRdy->OSTCBStkPtr;
;              h) Restore R4-R11 from new process stack;
;              i) Perform exception return which will restore remaining context.
;
;           3) On entry into PendSV handler:
;              a) The following have been saved on the process stack (by processor):
;                 xPSR, PC, LR, R12, R0-R3
;              b) Processor mode is switched to Handler mode (from Thread mode)
;              c) Stack is Main stack (switched from Process stack)
;              d) OSTCBCur      points to the OS_TCB of the task to suspend
;                 OSTCBHighRdy  points to the OS_TCB of the task to resume
;
;           4) Since PendSV is set to lowest priority in the system (by OSStartHighRdy() above), we
;              know that it will only be run when no other exception or interrupt is active, and
;              therefore safe to assume that context being switched out was using the process stack (PSP).
;********************************************************************************************************

OS_CPU_PendSVHandler
    CPSID   I                                                   ;关中断 Prevent interruption during context switch
    MRS     R0, PSP                                             ;获取PSP的数值(PSP存储着当前堆栈首地址) PSP is process stack pointer
    CBZ     R0, OS_CPU_PendSVHandler_nosave                     ;PSP=0(意味着之前没有任务,这是第一次任务切换，不需要保存数据)
                                                                ;则跳转到OS_CPU_PendSVHandler_nosave. Skip register save the first time

    SUBS    R0, R0, #0x20                                       ; cortex-M3会自动入栈保存数据，所以只需要存R4-R11寄存器一共32字节即可.
    STM     R0, {R4-R11}                                        ; 注意此时R0存储着栈地址即SP. Save remaining regs r4-11 on process stack

    LDR     R1, =OSTCBCur                                       ; OSTCBCur的地址存入R1. OSTCBCur->OSTCBStkPtr = SP;
    LDR     R1, [R1]                                            ; OSTCBCur的值存入R1.
    STR     R0, [R1]                                            ; 将R0的值(即SP)存入R1的地址,即将SP存入OSTCBCur的首地址中
                                                                ; R0 is SP of process being switched out
                                                                ; 至此为止,被切换任务堆栈保存完毕At this point, entire context of process has been saved

OS_CPU_PendSVHandler_nosave
    PUSH    {R14}                                               ; LR入栈 Save LR exc_return value
    LDR     R0, =OSTaskSwHook                                   ; 钩子函数的地址存入R0 OSTaskSwHook();
    BLX     R0                                                  ; 跳转执行钩子函数
    POP     {R14}                                               ; LR出栈,保证在跳转执行钩子函数的时候,LR中的值没有被破坏

    LDR     R0, =OSPrioCur                                      ; R0中存的是OSPrioCur的地址 OSPrioCur = OSPrioHighRdy;
    LDR     R1, =OSPrioHighRdy                                  ; R1中存的是OSPrioHighRdy的地址
    LDRB    R2, [R1]                                            ; OSPrioHighRdy的值存入R2
    STRB    R2, [R0]                                            ; R2中的值(OSPrioHighRdy)存入OSPrioCur

    LDR     R0, =OSTCBCur                                       ; 和上述同理  OSTCBCur  = OSTCBHighRdy;
    LDR     R1, =OSTCBHighRdy
    LDR     R2, [R1]
    STR     R2, [R0]                                            ; 此时R2存储着OSTCBHighRdy的地址,即新任务的堆栈地址

    LDR     R0, [R2]                                            ; 此时R0存储的是新任务的堆栈指针 R0 is new process SP; SP = OSTCBHighRdy->OSTCBStkPtr;
    LDM     R0, {R4-R11}                                        ; R4-R11出栈 Restore r4-11 from new process stack
    ADDS    R0, R0, #0x20                                       ; 出栈了8个寄存器，堆栈指针+32
    MSR     PSP, R0                                             ; 将堆栈的指针赋给PSP Load PSP with new process SP
    ORR     LR, LR, #0x04                                       ; 确保返回的LR值的低四位是X1XX Ensure exception return uses process stack
    CPSIE   I                                                   ; 开中断
    BX      LR                                                  ; 退出PendSV中断 Exception return will restore remaining context

    END
