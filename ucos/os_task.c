/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*                                            TASK MANAGEMENT
*
*                              (c) Copyright 1992-2009, Micrium, Weston, FL
*                                           All Rights Reserved
*
* File    : OS_TASK.C
* By      : Jean J. Labrosse
* Version : V2.91
*
* LICENSING TERMS:
* ---------------
*   uC/OS-II is provided in source form for FREE evaluation, for educational use or for peaceful research.
* If you plan on using  uC/OS-II  in a commercial product you need to contact Micri祄 to properly license
* its use in your product. We provide ALL the source code for your convenience and to help you experience
* uC/OS-II.   The fact that the  source is provided does  NOT  mean that you can use it without  paying a
* licensing fee.
*********************************************************************************************************
*/

#ifndef  OS_MASTER_FILE
#include <ucos_ii.h>
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                        CHANGE PRIORITY OF A TASK
*
* Description: This function allows you to change the priority of a task dynamically.  Note that the new
*              priority MUST be available.
*
* Arguments  : oldp     is the old priority
*
*              newp     is the new priority
*
* Returns    : OS_ERR_NONE            is the call was successful
*              OS_ERR_PRIO_INVALID    if the priority you specify is higher that the maximum allowed
*                                     (i.e. >= OS_LOWEST_PRIO)原优先级或新优先级非法
*              OS_ERR_PRIO_EXIST      if the new priority already exist.新优先级已经存在
*              OS_ERR_PRIO            there is no task with the specified OLD priority (i.e. the OLD task does
*                                     not exist.旧优先级不存在
*              OS_ERR_TASK_NOT_EXIST  if the task is assigned to a Mutex PIP. 如果任务被分配到一个互斥的继承优先级
*********************************************************************************************************
*/

#if OS_TASK_CHANGE_PRIO_EN > 0u
INT8U OSTaskChangePrio(INT8U oldprio, INT8U newprio)
{
#if (OS_EVENT_EN)
	OS_EVENT *pevent;
#if (OS_EVENT_MULTI_EN > 0u)
	OS_EVENT **pevents;
#endif
#endif
	OS_TCB *ptcb;
	INT8U y_new;
	INT8U x_new;
	INT8U y_old;
	OS_PRIO bity_new;
	OS_PRIO bitx_new;
	OS_PRIO bity_old;
	OS_PRIO bitx_old;
#if OS_CRITICAL_METHOD == 3u //采用第三种方式开关中断
	OS_CPU_SR cpu_sr = 0u;	/* Storage for CPU status register         */
#endif


/*$PAGE*/
#if OS_ARG_CHK_EN > 0u                  //确保输入优先级合法
	if (oldprio >= OS_LOWEST_PRIO) {
		if (oldprio != OS_PRIO_SELF) {
			return (OS_ERR_PRIO_INVALID);
		}
	}
	if (newprio >= OS_LOWEST_PRIO) {
		return (OS_ERR_PRIO_INVALID);
	}
#endif
	OS_ENTER_CRITICAL();
	if (OSTCBPrioTbl[newprio] != (OS_TCB *) 0) {	/* 新优先级不能存在 New priority must not already exist     */
		OS_EXIT_CRITICAL();
		return (OS_ERR_PRIO_EXIST);
	}
	if (oldprio == OS_PRIO_SELF) {	/*是不是改变当前任务的优先级 See if changing self                    */
		oldprio = OSTCBCur->OSTCBPrio;	/*是,获取当前任务的优先级 Yes, get priority                       */
	}
	ptcb = OSTCBPrioTbl[oldprio];   //获取任务的TCB
	if (ptcb == (OS_TCB *) 0) {	/* Does task to change exist?              */
		OS_EXIT_CRITICAL();	/* No, can't change its priority!          */
		return (OS_ERR_PRIO);   //任务不存在
	}
	if (ptcb == OS_TCB_RESERVED) {	/* Is task assigned to Mutex               */
		OS_EXIT_CRITICAL();	/* No, can't change its priority!          */
		return (OS_ERR_TASK_NOT_EXIST); //任务被分配互斥,不能改变优先级
	}
#if OS_LOWEST_PRIO <= 63u               //计算新的优先级在就绪表中的位置
	y_new = (INT8U) (newprio >> 3u);	/* Yes, compute new TCB fields             */
	x_new = (INT8U) (newprio & 0x07u);
#else
	y_new = (INT8U) ((INT8U) (newprio >> 4u) & 0x0Fu);
	x_new = (INT8U) (newprio & 0x0Fu);
#endif
	bity_new = (OS_PRIO) (1uL << y_new);
	bitx_new = (OS_PRIO) (1uL << x_new);

	OSTCBPrioTbl[oldprio] = (OS_TCB *) 0;	/*删除优先级表中旧优先级位置的TCB Remove TCB from old priority            */
	OSTCBPrioTbl[newprio] = ptcb;	/*将其加到新优先级位置上 Place pointer to TCB @ new priority     */
	y_old = ptcb->OSTCBY;
	bity_old = ptcb->OSTCBBitY;
	bitx_old = ptcb->OSTCBBitX;
	if ((OSRdyTbl[y_old] & bitx_old) != 0u) {	/*从旧优先级的就绪表中移除 If task is ready make it not            */
		OSRdyTbl[y_old] &= (OS_PRIO) ~ bitx_old;
		if (OSRdyTbl[y_old] == 0u) {
			OSRdyGrp &= (OS_PRIO) ~ bity_old;
		}
		OSRdyGrp |= bity_new;	/*加入新优先级的就绪表 Make new priority ready to run          */
		OSRdyTbl[y_new] |= bitx_new;
	}
#if (OS_EVENT_EN)
	pevent = ptcb->OSTCBEventPtr;
	if (pevent != (OS_EVENT *) 0) {
		pevent->OSEventTbl[y_old] &= (OS_PRIO) ~ bitx_old;	/*在等待表中将旧优先级移除 Remove old task prio from wait list     */
		if (pevent->OSEventTbl[y_old] == 0u) {
			pevent->OSEventGrp &= (OS_PRIO) ~ bity_old;
		}
		pevent->OSEventGrp |= bity_new;	/*加入等待表中新优先级位置 Add    new task prio to   wait list     */
		pevent->OSEventTbl[y_new] |= bitx_new;
	}
#if (OS_EVENT_MULTI_EN > 0u)
	if (ptcb->OSTCBEventMultiPtr != (OS_EVENT **) 0) {
		pevents = ptcb->OSTCBEventMultiPtr;
		pevent = *pevents;
		while (pevent != (OS_EVENT *) 0) {
			pevent->OSEventTbl[y_old] &= (OS_PRIO) ~ bitx_old;	/* Remove old task prio from wait lists */
			if (pevent->OSEventTbl[y_old] == 0u) {
				pevent->OSEventGrp &= (OS_PRIO) ~ bity_old;
			}
			pevent->OSEventGrp |= bity_new;	/* Add    new task prio to   wait lists    */
			pevent->OSEventTbl[y_new] |= bitx_new;
			pevents++;
			pevent = *pevents;
		}
	}
#endif
#endif

	ptcb->OSTCBPrio = newprio;	/*设置新任务优先级 Set new task priority                   */
	ptcb->OSTCBY = y_new;
	ptcb->OSTCBX = x_new;
	ptcb->OSTCBBitY = bity_new;
	ptcb->OSTCBBitX = bitx_new;
	OS_EXIT_CRITICAL();
	if (OSRunning == OS_TRUE) {
		OS_Sched();	/*重新调度 Find new highest priority task          */
	}
	return (OS_ERR_NONE);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                            CREATE A TASK
*
* Description: This function is used to have uC/OS-II manage the execution of a task.  Tasks can either
*              be created prior to the start of multitasking or by a running task.  A task cannot be
*              created by an ISR.任务不能被中断服务程序创建!!
*
* Arguments  : task     is a pointer to the task's code  任务代码指针
*
*              p_arg    is a pointer to an optional data area which can be used to pass parameters to
*                       the task when the task first executes.  Where the task is concerned it thinks
*                       it was invoked and passed the argument 'p_arg' as follows:
*
*                           void Task (void *p_arg)
*                           {
*                               for (;;) {
*                                   Task code;
*                               }
*                           }
*                       任务执行时传递给任务的参数的指针
*              ptos     is a pointer to the task's top of stack.  If the configuration constant
*                       OS_STK_GROWTH is set to 1, the stack is assumed to grow downward (i.e. from high
*                       memory to low memory).  'pstk' will thus point to the highest (valid) memory
*                       location of the stack.  If OS_STK_GROWTH is set to 0, 'pstk' will point to the
*                       lowest memory location of the stack and the stack will grow with increasing
*                       memory locations.
*                       分配给任务堆栈的栈顶指针
*              prio     is the task's priority.  A unique priority MUST be assigned to each task and the
*                       lower the number, the higher the priority.
*                       分配给任务的优先级
* Returns    : OS_ERR_NONE             if the function was successful.创建任务成功
*              OS_PRIO_EXIT            if the task priority already exist 指定的优先级已经被占用
*                                      (each task MUST have a unique priority).
*              OS_ERR_PRIO_INVALID     if the priority you specify is higher that the maximum allowed
*                                      (i.e. >= OS_LOWEST_PRIO)指定的优先级大于OS_LOWEST_PRIO
*              OS_ERR_TASK_CREATE_ISR  if you tried to create a task from an ISR.在中断服务程序中创建任务
*********************************************************************************************************
*/

#if OS_TASK_CREATE_EN > 0u
INT8U OSTaskCreate(void (*task) (void *p_arg), void *p_arg, OS_STK * ptos, INT8U prio)
{
	OS_STK *psp;
	INT8U err;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register               */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL_IEC61508
	if (OSSafetyCriticalStartFlag == OS_TRUE) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (prio > OS_LOWEST_PRIO) {	/*输入优先级是否合法 Make sure priority is within allowable range           */
		return (OS_ERR_PRIO_INVALID);
	}
#endif
	OS_ENTER_CRITICAL();
	if (OSIntNesting > 0u) {	/*是否处于中断服务程序 Make sure we don't create the task from within an ISR  */
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_CREATE_ISR);
	}
	if (OSTCBPrioTbl[prio] == (OS_TCB *) 0) {	/*优先级是否被占用 Make sure task doesn't already exist at this priority  */
		OSTCBPrioTbl[prio] = OS_TCB_RESERVED;	/*占用该优先级 Reserve the priority to prevent others from doing ...  */
		/* ... the same thing until task is created.              */
		OS_EXIT_CRITICAL();
		psp = OSTaskStkInit(task, p_arg, ptos, 0u);	/*建立任务栈,返回新栈顶 Initialize the task's stack         */
		err = OS_TCBInit(prio, psp, (OS_STK *) 0, 0u, 0u, (void *) 0, 0u);  //初始化任务控制块
		if (err == OS_ERR_NONE) {       //初始化任务控制块有没有错
			if (OSRunning == OS_TRUE) {	/*内核跑起来了吗 Find highest priority task if multitasking has started */
				OS_Sched();             //任务调度
			}
		} else {                                //初始化TCB有错
			OS_ENTER_CRITICAL();
			OSTCBPrioTbl[prio] = (OS_TCB *) 0;	/* 交还TCB Make this priority available to others                 */
			OS_EXIT_CRITICAL();
		}
		return (err);
	}
	OS_EXIT_CRITICAL();
	return (OS_ERR_PRIO_EXIST);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                     CREATE A TASK (Extended Version)
*
* Description: This function is used to have uC/OS-II manage the execution of a task.  Tasks can either
*              be created prior to the start of multitasking or by a running task.  A task cannot be
*              created by an ISR.  This function is similar to OSTaskCreate() except that it allows
*              additional information about a task to be specified.
*
* Arguments  : task      is a pointer to the task's code
*
*              p_arg     is a pointer to an optional data area which can be used to pass parameters to
*                        the task when the task first executes.  Where the task is concerned it thinks
*                        it was invoked and passed the argument 'p_arg' as follows:
*
*                            void Task (void *p_arg)
*                            {
*                                for (;;) {
*                                    Task code;
*                                }
*                            }
*
*              ptos      is a pointer to the task's top of stack.  If the configuration constant
*                        OS_STK_GROWTH is set to 1, the stack is assumed to grow downward (i.e. from high
*                        memory to low memory).  'ptos' will thus point to the highest (valid) memory
*                        location of the stack.  If OS_STK_GROWTH is set to 0, 'ptos' will point to the
*                        lowest memory location of the stack and the stack will grow with increasing
*                        memory locations.  'ptos' MUST point to a valid 'free' data item.
*
*              prio      is the task's priority.  A unique priority MUST be assigned to each task and the
*                        lower the number, the higher the priority.
*
*              id        is the task's ID (0..65535),用于拓展支持的任务数目大于64
*
*              pbos      is a pointer to the task's bottom of stack.  If the configuration constant
*                        OS_STK_GROWTH is set to 1, the stack is assumed to grow downward (i.e. from high
*                        memory to low memory).  'pbos' will thus point to the LOWEST (valid) memory
*                        location of the stack.  If OS_STK_GROWTH is set to 0, 'pbos' will point to the
*                        HIGHEST memory location of the stack and the stack will grow with increasing
*                        memory locations.  'pbos' MUST point to a valid 'free' data item.
*                        指向任务的堆栈栈底的指针,用于堆栈检验
*
*              stk_size  is the size of the stack in number of elements.  If OS_STK is set to INT8U,
*                        'stk_size' corresponds to the number of bytes available.  If OS_STK is set to
*                        INT16U, 'stk_size' contains the number of 16-bit entries available.  Finally, if
*                        OS_STK is set to INT32U, 'stk_size' contains the number of 32-bit entries
*                        available on the stack.
*                        堆栈成员数目的容量�,单位是堆栈入口地址宽度
*
*              pext      is a pointer to a user supplied memory location which is used as a TCB extension.
*                        For example, this user memory can hold the contents of floating-point registers
*                        during a context switch, the time each task takes to execute, the number of times
*                        the task has been switched-in, etc.
*                        用户附加数据结构的指针，用于拓展OS_TCB
*
*              opt       contains additional information (or options) about the behavior of the task.  The
*                        LOWER 8-bits are reserved by uC/OS-II while the upper 8 bits can be application
*                        specific.  See OS_TASK_OPT_??? in uCOS-II.H.  Current choices are:
*
*                        OS_TASK_OPT_STK_CHK      Stack checking to be allowed for the task 允许堆栈检验
*                        OS_TASK_OPT_STK_CLR      Clear the stack when the task is created 允许堆栈清零
*                        OS_TASK_OPT_SAVE_FP      If the CPU has floating-point registers, save them
*                                                 during a context switch.  允许任务做浮点运算
*
* Returns    : OS_ERR_NONE             if the function was successful.
*              OS_PRIO_EXIT            if the task priority already exist
*                                      (each task MUST have a unique priority).
*              OS_ERR_PRIO_INVALID     if the priority you specify is higher that the maximum allowed
*                                      (i.e. > OS_LOWEST_PRIO)
*              OS_ERR_TASK_CREATE_ISR  if you tried to create a task from an ISR.
*********************************************************************************************************
*/
/*$PAGE*/
#if OS_TASK_CREATE_EXT_EN > 0u
INT8U OSTaskCreateExt(void (*task) (void *p_arg),
		      void *p_arg,
		      OS_STK * ptos, INT8U prio, INT16U id, OS_STK * pbos, INT32U stk_size, void *pext, INT16U opt)
{
	OS_STK *psp;
	INT8U err;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register               */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式实现开关中断,需要cpu_sr来存储中断状态
#endif



#ifdef OS_SAFETY_CRITICAL_IEC61508
	if (OSSafetyCriticalStartFlag == OS_TRUE) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (prio > OS_LOWEST_PRIO) {	/*检查优先级的合法性 Make sure priority is within allowable range           */
		return (OS_ERR_PRIO_INVALID);
	}
#endif
	OS_ENTER_CRITICAL();
	if (OSIntNesting > 0u) {	/*是否在中断服务程序中 Make sure we don't create the task from within an ISR  */
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_CREATE_ISR);
	}
	if (OSTCBPrioTbl[prio] == (OS_TCB *) 0) {	/*优先级是否被占用 Make sure task doesn't already exist at this priority  */
		OSTCBPrioTbl[prio] = OS_TCB_RESERVED;	/*占用该优先级 Reserve the priority to prevent others from doing ...  */
		/* ... the same thing until task is created.              */
		OS_EXIT_CRITICAL();

#if (OS_TASK_STAT_STK_CHK_EN > 0u)
		OS_TaskStkClr(pbos, stk_size, opt);	/*任务堆栈清零 Clear the task stack (if needed)     */
#endif

		psp = OSTaskStkInit(task, p_arg, ptos, opt);	/*初始化任务栈,返回新栈顶 Initialize the task's stack          */
		err = OS_TCBInit(prio, psp, pbos, id, stk_size, pext, opt); //初始化TCB
		if (err == OS_ERR_NONE) {       //初始化TCB成功
			if (OSRunning == OS_TRUE) {	/*内核运行么 Find HPT if multitasking has started */
				OS_Sched();             //任务调度
			}
		} else {                                //初始化TCB没成功
			OS_ENTER_CRITICAL();
			OSTCBPrioTbl[prio] = (OS_TCB *) 0;	/* 交还TCB Make this priority avail. to others  */
			OS_EXIT_CRITICAL();
		}
		return (err);
	}
	OS_EXIT_CRITICAL();
	return (OS_ERR_PRIO_EXIST);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                            DELETE A TASK
*
* Description: This function allows you to delete a task.  The calling task can delete itself by
*              its own priority number.  The deleted task is returned to the dormant state and can be
*              re-activated by creating the deleted task again.删除任务的功能不是删除任务代码，而是使代码
*              返回并处于休眠态,任务的代码将不会被OS调用，除非重新启动.
*
* Arguments  : prio    is the priority of the task to delete.  Note that you can explicitely delete
*                      the current task without knowing its priority level by setting 'prio' to
*                      OS_PRIO_SELF.
*
* Returns    : OS_ERR_NONE             if the call is successful 任务删除成功
*              OS_ERR_TASK_DEL_IDLE    if you attempted to delete uC/OS-II's idle task 试图删除空闲任务
*              OS_ERR_PRIO_INVALID     if the priority you specify is higher that the maximum allowed给出的优先级非法
*                                      (i.e. >= OS_LOWEST_PRIO) or, you have not specified OS_PRIO_SELF.
*              OS_ERR_TASK_DEL         if the task is assigned to a Mutex PIP.任务被分配一个互斥的继承优先级
*              OS_ERR_TASK_NOT_EXIST   if the task you want to delete does not exist.删除的任务不存在
*              OS_ERR_TASK_DEL_ISR     if you tried to delete a task from an ISR试图在中断处理程序中删除任务
*
* Notes      : 1) To reduce interrupt latency, OSTaskDel() 'disables' the task:
*                    a) by making it not ready
*                    b) by removing it from any wait lists
*                    c) by preventing OSTimeTick() from making the task ready to run.
*                 The task can then be 'unlinked' from the miscellaneous structures in uC/OS-II.
*              2) The function OS_Dummy() is called after OS_EXIT_CRITICAL() because, on most processors,
*                 the next instruction following the enable interrupt instruction is ignored.
*              3) An ISR cannot delete a task.
*              4) The lock nesting counter is incremented because, for a brief instant, if the current
*                 task is being deleted, the current task would not be able to be rescheduled because it
*                 is removed from the ready list.  Incrementing the nesting counter prevents another task
*                 from being schedule.  This means that an ISR would return to the current task which is
*                 being deleted.  The rest of the deletion would thus be able to be completed.
*********************************************************************************************************
*/

#if OS_TASK_DEL_EN > 0u
INT8U OSTaskDel(INT8U prio)
{
#if (OS_FLAG_EN > 0u) && (OS_MAX_FLAGS > 0u)
	OS_FLAG_NODE *pnode;
#endif
	OS_TCB *ptcb;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register    */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



	if (OSIntNesting > 0u) {	/* 是否在中断服务程序中See if trying to delete from ISR            */
		return (OS_ERR_TASK_DEL_ISR);
	}
	if (prio == OS_TASK_IDLE_PRIO) {	/* 删除的是否是空闲任务Not allowed to delete idle task             */
		return (OS_ERR_TASK_DEL_IDLE);
	}
#if OS_ARG_CHK_EN > 0u
	if (prio >= OS_LOWEST_PRIO) {	/* 输入的优先级是否合法Task priority valid ?                       */
		if (prio != OS_PRIO_SELF) {
			return (OS_ERR_PRIO_INVALID);
		}
	}
#endif

/*$PAGE*/
	OS_ENTER_CRITICAL();
	if (prio == OS_PRIO_SELF) {	/* 删除是自身么 See if requesting to delete self            */
		prio = OSTCBCur->OSTCBPrio;	/* 允许删除自身 Set priority to delete to current           */
	}
	ptcb = OSTCBPrioTbl[prio];  //指向被删除任务的TCB
	if (ptcb == (OS_TCB *) 0) {	/* 确保被删除的TCB是存在的 Task to delete must exist                   */
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_NOT_EXIST);
	}
	if (ptcb == OS_TCB_RESERVED) {	/*被删除的任务不能被分配互斥 Must not be assigned to Mutex               */
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_DEL);
	}

	OSRdyTbl[ptcb->OSTCBY] &= (OS_PRIO) ~ ptcb->OSTCBBitX;
	if (OSRdyTbl[ptcb->OSTCBY] == 0u) {	/*若任务处于就绪表中(上一句将其复位了) Make task not ready                         */
		OSRdyGrp &= (OS_PRIO) ~ ptcb->OSTCBBitY;    //将任务移出就绪表
	}
#if (OS_EVENT_EN)
	if (ptcb->OSTCBEventPtr != (OS_EVENT *) 0) {//如果任务在等待消息、信号量等，将其从等待列表中删除
		OS_EventTaskRemove(ptcb, ptcb->OSTCBEventPtr);	/* Remove this task from any event   wait list */
	}
#if (OS_EVENT_MULTI_EN > 0u)
	if (ptcb->OSTCBEventMultiPtr != (OS_EVENT **) 0) {	/* Remove this task from any events' wait lists */
		OS_EventTaskRemoveMulti(ptcb, ptcb->OSTCBEventMultiPtr);
	}
#endif
#endif

#if (OS_FLAG_EN > 0u) && (OS_MAX_FLAGS > 0u)
	pnode = ptcb->OSTCBFlagNode;
	if (pnode != (OS_FLAG_NODE *) 0) {	/*任务处于事件标志等待列表中 If task is waiting on event flag            */
		OS_FlagUnlink(pnode);	/*将其从等待链表中删除 Remove from wait list                       */
	}
#endif

	ptcb->OSTCBDly = 0u;	/*将延时数清零,确保重开中断后,ISR不再会使该任务就绪 Prevent OSTimeTick() from updating          */
	ptcb->OSTCBStat = OS_STAT_RDY;	/*设置TCB状态，防止调用OSTaskResume使其重新激活 Prevent task from being resumed             */
	ptcb->OSTCBStatPend = OS_STAT_PEND_OK; //设置pend完成标志
	//执行完毕上述过程后,被删除的任务既不再等待延时期满,也不会出现在就绪表和各种任务等待列表中
	//内核再也找不到它了,所以它就不会再被其他任务或者ISR置于就绪态
	if (OSLockNesting < 255u) {	/* Make sure we don't context switch           */
		OSLockNesting++;        //锁住调度器
	}
	OS_EXIT_CRITICAL();	/*因为中断关闭的太久,开放一次中断,缩短中断响应 Enabling INT. ignores next instruc.         */
	OS_Dummy();		/* 执行一条空指令，增加中断进入的机会 ... Dummy ensures that INTs will be         */
	OS_ENTER_CRITICAL();	/*重新关闭中断 ... disabled HERE!                          */

	if (OSLockNesting > 0u) {	/* Remove context switch lock                  */
		OSLockNesting--;        //解锁调度器
	}

	OSTaskDelHook(ptcb);	/* 调用扩展钩子函数,可以利用它删除或释放自定义的TCB附加数据结构 Call user defined hook                      */
	OSTaskCtr--;		/*任务数量计数器减一 One less task being managed                 */
	OSTCBPrioTbl[prio] = (OS_TCB *) 0;	/*从优先级列表删除OS_TCB Clear old priority entry                    */
	if (ptcb->OSTCBPrev == (OS_TCB *) 0) {	/*在双向链表中删除TCB Remove from TCB chain                       */
		ptcb->OSTCBNext->OSTCBPrev = (OS_TCB *) 0; //如果前面没有TCB了
		OSTCBList = ptcb->OSTCBNext;
	} else {
		ptcb->OSTCBPrev->OSTCBNext = ptcb->OSTCBNext;
		ptcb->OSTCBNext->OSTCBPrev = ptcb->OSTCBPrev;
	}
	ptcb->OSTCBNext = OSTCBFreeList;	/*将被删除的TCB放回到空闲链表中 Return TCB to free TCB list                 */
	OSTCBFreeList = ptcb;
#if OS_TASK_NAME_EN > 0u
	ptcb->OSTCBTaskName = (INT8U *) (void *) "?";   //名字也收回，改成?
#endif
	OS_EXIT_CRITICAL();
	if (OSRunning == OS_TRUE) {
		OS_Sched();	/*重新触发任务调度 Find new highest priority task              */
	}
	return (OS_ERR_NONE);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                    REQUEST THAT A TASK DELETE ITSELF
*
* Description: This function is used to:
*                   a) notify a task to delete itself.
*                   b) to see if a task requested that the current task delete itself.
*              This function is a little tricky to understand.  Basically, you have a task that needs
*              to be deleted however, this task has resources that it has allocated (memory buffers,
*              semaphores, mailboxes, queues etc.).  The task cannot be deleted otherwise these
*              resources would not be freed.  The requesting task calls OSTaskDelReq() to indicate that
*              the task needs to be deleted.  Deleting of the task is however, deferred to the task to
*              be deleted.  For example, suppose that task #10 needs to be deleted.  The requesting task
*              example, task #5, would call OSTaskDelReq(10).  When task #10 gets to execute, it calls
*              this function by specifying OS_PRIO_SELF and monitors the returned value.  If the return
*              value is OS_ERR_TASK_DEL_REQ, another task requested a task delete.  Task #10 would look like
*              this:
*
*                   void Task(void *p_arg)
*                   {
*                       .
*                       .
*                       while (1) {
*                           OSTimeDly(1);
*                           if (OSTaskDelReq(OS_PRIO_SELF) == OS_ERR_TASK_DEL_REQ) {
*                               Release any owned resources;
*                               De-allocate any dynamic memory;
*                               OSTaskDel(OS_PRIO_SELF);
*                           }
*                       }
*                   }
*              通知让那些占有资源的任务先释放任务再删除
*
* Arguments  : prio    is the priority of the task to request the delete from
*
* Returns    : OS_ERR_NONE            if the task exist and the request has been registered 调用成功
*              OS_ERR_TASK_NOT_EXIST  if the task has been deleted.  This allows the caller to know whether
*                                     the request has been executed. 指定的任务不存在
*              OS_ERR_TASK_DEL        if the task is assigned to a Mutex. 任务被分配互斥
*              OS_ERR_TASK_DEL_IDLE   if you requested to delete uC/OS-II's idle task 试图删除空任务
*              OS_ERR_PRIO_INVALID    if the priority you specify is higher that the maximum allowed 输入的优先级非法
*                                     (i.e. >= OS_LOWEST_PRIO) or, you have not specified OS_PRIO_SELF.
*              OS_ERR_TASK_DEL_REQ    if a task (possibly another task) requested that the running task be
*                                     deleted. 当前任务收到其他任务的删除请求
*********************************************************************************************************
*/
/*$PAGE*/
#if OS_TASK_DEL_EN > 0u
INT8U OSTaskDelReq(INT8U prio)
{
	INT8U stat;
	OS_TCB *ptcb;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //使用方式三来开关中断,通过定义cpu_sr来保存中断状态
#endif



	if (prio == OS_TASK_IDLE_PRIO) {	/*删除的是空闲任务么 Not allowed to delete idle task     */
		return (OS_ERR_TASK_DEL_IDLE);
	}
#if OS_ARG_CHK_EN > 0u
	if (prio >= OS_LOWEST_PRIO) {	/* 输入的优先级合法么 Task priority valid ?               */
		if (prio != OS_PRIO_SELF) {
			return (OS_ERR_PRIO_INVALID);
		}
	}
#endif
	if (prio == OS_PRIO_SELF) {	/*是自我删除么 See if a task is requesting to ...  */
		OS_ENTER_CRITICAL();	/* ... this task to delete itself      */
		stat = OSTCBCur->OSTCBDelReq;	/* Return request status to caller     */
		OS_EXIT_CRITICAL();
		return (stat);
	}
	OS_ENTER_CRITICAL();
	ptcb = OSTCBPrioTbl[prio];  // 指针指向相应的TCB
	if (ptcb == (OS_TCB *) 0) {	/* TCB是否存在 Task to delete must exist           */
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_NOT_EXIST);	/* Task must already be deleted        */
	}
	if (ptcb == OS_TCB_RESERVED) {	/*是否被分配了互斥  Must NOT be assigned to a Mutex     */
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_DEL);
	}
	ptcb->OSTCBDelReq = OS_ERR_TASK_DEL_REQ;	/*设置TCB相应的数据块结构 Set flag indicating task to be DEL. */
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                        GET THE NAME OF A TASK
*
* Description: This function is called to obtain the name of a task.
*
* Arguments  : prio      is the priority of the task that you want to obtain the name from.
*
*              pname     is a pointer to a pointer to an ASCII string that will receive the name of the task.
*
*              perr      is a pointer to an error code that can contain one of the following values:
*
*                        OS_ERR_NONE                if the requested task is resumed
*                        OS_ERR_TASK_NOT_EXIST      if the task has not been created or is assigned to a Mutex
*                        OS_ERR_PRIO_INVALID        if you specified an invalid priority:
*                                                   A higher value than the idle task or not OS_PRIO_SELF.
*                        OS_ERR_PNAME_NULL          You passed a NULL pointer for 'pname'
*                        OS_ERR_NAME_GET_ISR        You called this function from an ISR
*
*
* Returns    : The length of the string or 0 if the task does not exist.
*********************************************************************************************************
*/

#if OS_TASK_NAME_EN > 0u
INT8U OSTaskNameGet(INT8U prio, INT8U ** pname, INT8U * perr)
{
	OS_TCB *ptcb;
	INT8U len;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register   */
	OS_CPU_SR cpu_sr = 0u;
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (prio > OS_LOWEST_PRIO) {	/* Task priority valid ?                      */
		if (prio != OS_PRIO_SELF) {
			*perr = OS_ERR_PRIO_INVALID;	/* No                                         */
			return (0u);
		}
	}
	if (pname == (INT8U **) 0) {	/* Is 'pname' a NULL pointer?                 */
		*perr = OS_ERR_PNAME_NULL;	/* Yes                                        */
		return (0u);
	}
#endif
	if (OSIntNesting > 0u) {	/* See if trying to call from an ISR          */
		*perr = OS_ERR_NAME_GET_ISR;
		return (0u);
	}
	OS_ENTER_CRITICAL();
	if (prio == OS_PRIO_SELF) {	/* See if caller desires it's own name        */
		prio = OSTCBCur->OSTCBPrio;
	}
	ptcb = OSTCBPrioTbl[prio];
	if (ptcb == (OS_TCB *) 0) {	/* Does task exist?                           */
		OS_EXIT_CRITICAL();	/* No                                         */
		*perr = OS_ERR_TASK_NOT_EXIST;
		return (0u);
	}
	if (ptcb == OS_TCB_RESERVED) {	/* Task assigned to a Mutex?                  */
		OS_EXIT_CRITICAL();	/* Yes                                        */
		*perr = OS_ERR_TASK_NOT_EXIST;
		return (0u);
	}
	*pname = ptcb->OSTCBTaskName;
	len = OS_StrLen(*pname);
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
	return (len);
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                        ASSIGN A NAME TO A TASK
*
* Description: This function is used to set the name of a task.
*
* Arguments  : prio      is the priority of the task that you want the assign a name to.
*
*              pname     is a pointer to an ASCII string that contains the name of the task.
*
*              perr       is a pointer to an error code that can contain one of the following values:
*
*                        OS_ERR_NONE                if the requested task is resumed
*                        OS_ERR_TASK_NOT_EXIST      if the task has not been created or is assigned to a Mutex
*                        OS_ERR_PNAME_NULL          You passed a NULL pointer for 'pname'
*                        OS_ERR_PRIO_INVALID        if you specified an invalid priority:
*                                                   A higher value than the idle task or not OS_PRIO_SELF.
*                        OS_ERR_NAME_SET_ISR        if you called this function from an ISR
*
* Returns    : None
*********************************************************************************************************
*/
#if OS_TASK_NAME_EN > 0u
void OSTaskNameSet(INT8U prio, INT8U * pname, INT8U * perr)
{
	OS_TCB *ptcb;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register       */
	OS_CPU_SR cpu_sr = 0u;
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (prio > OS_LOWEST_PRIO) {	/* Task priority valid ?                          */
		if (prio != OS_PRIO_SELF) {
			*perr = OS_ERR_PRIO_INVALID;	/* No                                             */
			return;
		}
	}
	if (pname == (INT8U *) 0) {	/* Is 'pname' a NULL pointer?                     */
		*perr = OS_ERR_PNAME_NULL;	/* Yes                                            */
		return;
	}
#endif
	if (OSIntNesting > 0u) {	/* See if trying to call from an ISR              */
		*perr = OS_ERR_NAME_SET_ISR;
		return;
	}
	OS_ENTER_CRITICAL();
	if (prio == OS_PRIO_SELF) {	/* See if caller desires to set it's own name     */
		prio = OSTCBCur->OSTCBPrio;
	}
	ptcb = OSTCBPrioTbl[prio];
	if (ptcb == (OS_TCB *) 0) {	/* Does task exist?                               */
		OS_EXIT_CRITICAL();	/* No                                             */
		*perr = OS_ERR_TASK_NOT_EXIST;
		return;
	}
	if (ptcb == OS_TCB_RESERVED) {	/* Task assigned to a Mutex?                      */
		OS_EXIT_CRITICAL();	/* Yes                                            */
		*perr = OS_ERR_TASK_NOT_EXIST;
		return;
	}
	ptcb->OSTCBTaskName = pname;
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                        RESUME A SUSPENDED TASK
*
* Description: This function is called to resume a previously suspended task.  This is the only call that
*              will remove an explicit task suspension.
*              唯一可以"解挂"挂起任务的函数
*
* Arguments  : prio     is the priority of the task to resume.
*
* Returns    : OS_ERR_NONE                if the requested task is resumed
*              OS_ERR_PRIO_INVALID        if the priority you specify is higher that the maximum allowed
*                                         (i.e. >= OS_LOWEST_PRIO)
*              OS_ERR_TASK_RESUME_PRIO    if the task to resume does not exist
*              OS_ERR_TASK_NOT_EXIST      if the task is assigned to a Mutex PIP
*              OS_ERR_TASK_NOT_SUSPENDED  if the task to resume has not been suspended
*********************************************************************************************************
*/

#if OS_TASK_SUSPEND_EN > 0u
INT8U OSTaskResume(INT8U prio)
{
	OS_TCB *ptcb;
#if OS_CRITICAL_METHOD == 3u	/* Storage for CPU status register       */
	OS_CPU_SR cpu_sr = 0u;      //第三种方式实现的开关中断,需要cpu_sr来存储中断状态
#endif



#if OS_ARG_CHK_EN > 0u
	if (prio >= OS_LOWEST_PRIO) {	/* Make sure task priority is valid      */
		return (OS_ERR_PRIO_INVALID);
	}
#endif
	OS_ENTER_CRITICAL();
	ptcb = OSTCBPrioTbl[prio];
	if (ptcb == (OS_TCB *) 0) {	/* Task to suspend must exist            */
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_RESUME_PRIO);
	}
	if (ptcb == OS_TCB_RESERVED) {	/* See if assigned to Mutex              */
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_NOT_EXIST);
	}
	if ((ptcb->OSTCBStat & OS_STAT_SUSPEND) != OS_STAT_RDY) {	/*任务是不是被挂起了 Task must be suspended                */
		ptcb->OSTCBStat &= (INT8U) ~ (INT8U) OS_STAT_SUSPEND;	/*清除挂起标志 Remove suspension                     */
		if (ptcb->OSTCBStat == OS_STAT_RDY) {	/*任务标志是否就绪 See if task is now ready              */
			if (ptcb->OSTCBDly == 0u) {         //有没有延时
				OSRdyGrp |= ptcb->OSTCBBitY;	/*将任务加入就绪表 Yes, Make task ready to run           */
				OSRdyTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
				OS_EXIT_CRITICAL();
				if (OSRunning == OS_TRUE) {
					OS_Sched();	/* Find new highest priority task        */
				}
			} else {
				OS_EXIT_CRITICAL();
			}
		} else {	/* Must be pending on event              */
			OS_EXIT_CRITICAL();
		}
		return (OS_ERR_NONE);
	}
	OS_EXIT_CRITICAL();
	return (OS_ERR_TASK_NOT_SUSPENDED);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                             STACK CHECKING
*
* Description: This function is called to check the amount of free memory left on the specified task's
*              stack.
*
* Arguments  : prio          is the task priority
*
*              p_stk_data    is a pointer to a data structure of type OS_STK_DATA.
*              其成员如下:
*              INT32U       OSFree      堆栈中未使用的字节数
*              INT32U       OSUsed      堆栈中已使用的字节数
*
* Returns    : OS_ERR_NONE            upon success
*              OS_ERR_PRIO_INVALID    if the priority you specify is higher that the maximum allowed
*                                     (i.e. > OS_LOWEST_PRIO) or, you have not specified OS_PRIO_SELF.
*              OS_ERR_TASK_NOT_EXIST  if the desired task has not been created or is assigned to a Mutex PIP
*              OS_ERR_TASK_OPT        if you did NOT specified OS_TASK_OPT_STK_CHK when the task was created
*              OS_ERR_PDATA_NULL      if 'p_stk_data' is a NULL pointer
*********************************************************************************************************
*/
#if (OS_TASK_STAT_STK_CHK_EN > 0u) && (OS_TASK_CREATE_EXT_EN > 0u)  //必须是通过OSTaskCreateExt建立的任务才可以统计
INT8U OSTaskStkChk(INT8U prio, OS_STK_DATA * p_stk_data)
{
	OS_TCB *ptcb;
	OS_STK *pchk;
	INT32U nfree;
	INT32U size;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register     */
	OS_CPU_SR cpu_sr = 0u;      //第三种方式来开关中断，需要cpu_sr来保存中断状态
#endif



#if OS_ARG_CHK_EN > 0u
	if (prio > OS_LOWEST_PRIO) {	/*优先级是否合法 Make sure task priority is valid             */
		if (prio != OS_PRIO_SELF) {
			return (OS_ERR_PRIO_INVALID);
		}
	}
	if (p_stk_data == (OS_STK_DATA *) 0) {	/*保存检验结果的数据格式不能为空 Validate 'p_stk_data'                        */
		return (OS_ERR_PDATA_NULL);
	}
#endif
	p_stk_data->OSFree = 0u;	/*初始化 Assume failure, set to 0 size                */
	p_stk_data->OSUsed = 0u;
	OS_ENTER_CRITICAL();
	if (prio == OS_PRIO_SELF) {	/*将OS_PRIO_SELF换算成优先级数值 See if check for SELF                        */
		prio = OSTCBCur->OSTCBPrio;
	}
	ptcb = OSTCBPrioTbl[prio];
	if (ptcb == (OS_TCB *) 0) {	/* Make sure task exist                         */
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_NOT_EXIST);
	}
	if (ptcb == OS_TCB_RESERVED) {
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_NOT_EXIST);
	}
	if ((ptcb->OSTCBOpt & OS_TASK_OPT_STK_CHK) == 0u) {	/* Make sure stack checking option is set      */
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_OPT);
	}
	nfree = 0u;
	size = ptcb->OSTCBStkSize;      //任务栈的容量
	pchk = ptcb->OSTCBStkBottom;    //任务栈底指针
	OS_EXIT_CRITICAL();
#if OS_STK_GROWTH == 1u             //自上向下生长
	while (*pchk++ == (OS_STK) 0) {	/* Compute the number of zero entries on the stk */
		nfree++;                    //统计堆栈中存储值为零的连续空间
	}
#else
	while (*pchk-- == (OS_STK) 0) {
		nfree++;
	}
#endif
    //这里得到的数据都是堆栈指针宽度为单位的长度,乘以4恢复字节为单位
	p_stk_data->OSFree = nfree * sizeof(OS_STK);	/* Compute number of free bytes on the stack */
	p_stk_data->OSUsed = (size - nfree) * sizeof(OS_STK);	/* Compute number of bytes used on the stack */
	return (OS_ERR_NONE);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                            SUSPEND A TASK
*
* Description: This function is called to suspend a task.  The task can be the calling task if the
*              priority passed to OSTaskSuspend() is the priority of the calling task or OS_PRIO_SELF.
*
* Arguments  : prio     is the priority of the task to suspend.  If you specify OS_PRIO_SELF, the
*                       calling task will suspend itself and rescheduling will occur.
*
* Returns    : OS_ERR_NONE               if the requested task is suspended
*              OS_ERR_TASK_SUSPEND_IDLE  if you attempted to suspend the idle task which is not allowed.
*              OS_ERR_PRIO_INVALID       if the priority you specify is higher that the maximum allowed
*                                        (i.e. >= OS_LOWEST_PRIO) or, you have not specified OS_PRIO_SELF.
*              OS_ERR_TASK_SUSPEND_PRIO  if the task to suspend does not exist
*              OS_ERR_TASK_NOT_EXITS     if the task is assigned to a Mutex PIP
*
* Note       : You should use this function with great care.  If you suspend a task that is waiting for
*              an event (i.e. a message, a semaphore, a queue ...) you will prevent this task from
*              running when the event arrives.
*              如果挂起一个在等待事件(消息、信号量等)的任务,当事件发生时,任务并不会被执行(还在挂起呢)
*********************************************************************************************************
*/

#if OS_TASK_SUSPEND_EN > 0u
INT8U OSTaskSuspend(INT8U prio)
{
	BOOLEAN self;
	OS_TCB *ptcb;
	INT8U y;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断,需要cpu_sr来保存中断状态
#endif



#if OS_ARG_CHK_EN > 0u
	if (prio == OS_TASK_IDLE_PRIO) {	/*不能删除空任务 Not allowed to suspend idle task    */
		return (OS_ERR_TASK_SUSPEND_IDLE);
	}
	if (prio >= OS_LOWEST_PRIO) {	/*输入优先级非法 Task priority valid ?               */
		if (prio != OS_PRIO_SELF) {
			return (OS_ERR_PRIO_INVALID);
		}
	}
#endif
	OS_ENTER_CRITICAL();
	if (prio == OS_PRIO_SELF) {	/*检查是否挂起自身 See if suspend SELF                 */
		prio = OSTCBCur->OSTCBPrio; //将OS_PRIO_SELF换算成优先级值
		self = OS_TRUE;         //设置自我挂起时的调度变量标志,以便在自我挂起后进行任务切换
	} else if (prio == OSTCBCur->OSTCBPrio) {	/* See if suspending self              */
		self = OS_TRUE;
	} else {                //不挂起自身,以后不进行任务切换
		self = OS_FALSE;	/* No suspending another task          */
	}
	ptcb = OSTCBPrioTbl[prio];
	if (ptcb == (OS_TCB *) 0) {	/* 被挂起的任务是存在的 Task to suspend must exist          */
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_SUSPEND_PRIO);
	}
	if (ptcb == OS_TCB_RESERVED) {	/* See if assigned to Mutex            */
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_NOT_EXIST);
	}
	y = ptcb->OSTCBY;           //任务存在,从就绪表中删除
	OSRdyTbl[y] &= (OS_PRIO) ~ ptcb->OSTCBBitX;	/* Make task not ready                 */
	if (OSRdyTbl[y] == 0u) {
		OSRdyGrp &= (OS_PRIO) ~ ptcb->OSTCBBitY;
	}
	ptcb->OSTCBStat |= OS_STAT_SUSPEND;	/*设置标志,标示被挂起 Status of task is 'SUSPENDED'       */
	OS_EXIT_CRITICAL();
	if (self == OS_TRUE) {	/*如果挂起自身,进行一次任务调度 Context switch only if SELF         */
		OS_Sched();	/* Find new highest priority task      */
	}
	return (OS_ERR_NONE);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                            QUERY A TASK
*
* Description: This function is called to obtain a copy of the desired task's TCB.
*              任务信息获取
*
* Arguments  : prio         is the priority of the task to obtain information from.
*
*              p_task_data  is a pointer to where the desired task's OS_TCB will be stored.
*                           保存返回的任务TCB的一个拷贝
*
* Returns    : OS_ERR_NONE            if the requested task is suspended
*              OS_ERR_PRIO_INVALID    if the priority you specify is higher that the maximum allowed
*                                     (i.e. > OS_LOWEST_PRIO) or, you have not specified OS_PRIO_SELF.
*              OS_ERR_PRIO            if the desired task has not been created
*              OS_ERR_TASK_NOT_EXIST  if the task is assigned to a Mutex PIP
*              OS_ERR_PDATA_NULL      if 'p_task_data' is a NULL pointer
*              调用时，请小心处理OSTCBNext和OSTCBPrev指针，不要去改变它们.
*********************************************************************************************************
*/

#if OS_TASK_QUERY_EN > 0u
INT8U OSTaskQuery(INT8U prio, OS_TCB * p_task_data)
{
	OS_TCB *ptcb;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //第三种方式实现开关中断,需要cpu_sr来保存中断状态
#endif



#if OS_ARG_CHK_EN > 0u
	if (prio > OS_LOWEST_PRIO) {	/* Task priority valid ?                              */
		if (prio != OS_PRIO_SELF) {
			return (OS_ERR_PRIO_INVALID);
		}
	}
	if (p_task_data == (OS_TCB *) 0) {	/* Validate 'p_task_data'                             */
		return (OS_ERR_PDATA_NULL);
	}
#endif
	OS_ENTER_CRITICAL();
	if (prio == OS_PRIO_SELF) {	/* See if suspend SELF                                */
		prio = OSTCBCur->OSTCBPrio;
	}
	ptcb = OSTCBPrioTbl[prio];
	if (ptcb == (OS_TCB *) 0) {	/* Task to query must exist                           */
		OS_EXIT_CRITICAL();
		return (OS_ERR_PRIO);
	}
	if (ptcb == OS_TCB_RESERVED) {	/* Task to query must not be assigned to a Mutex      */
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_NOT_EXIST);
	}
	/* Copy TCB into user storage area                    */
	OS_MemCopy((INT8U *) p_task_data, (INT8U *) ptcb, sizeof(OS_TCB));
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                 GET THE CURRENT VALUE OF A TASK REGISTER
*
* Description: This function is called to obtain the current value of a task register.  Task registers
*              are application specific and can be used to store task specific values such as 'error
*              numbers' (i.e. errno), statistics, etc.  Each task register can hold a 32-bit value.
*
* Arguments  : prio      is the priority of the task you want to get the task register from.  If you
*                        specify OS_PRIO_SELF then the task register of the current task will be obtained.
*
*              id        is the 'id' of the desired task register.  Note that the 'id' must be less
*                        than OS_TASK_REG_TBL_SIZE
*                        相应的寄存器id，必须小于OS_TASK_REG_TBL_SIZE
*
*              perr      is a pointer to a variable that will hold an error code related to this call.
*
*                        OS_ERR_NONE            if the call was successful
*                        OS_ERR_PRIO_INVALID    if you specified an invalid priority
*                        OS_ERR_ID_INVALID      if the 'id' is not between 0 and OS_TASK_REG_TBL_SIZE-1
*
* Returns    : The current value of the task's register or 0 if an error is detected.
*
* Note(s)    : The maximum number of task variables is 254
*********************************************************************************************************
*/

#if OS_TASK_REG_TBL_SIZE > 0u
INT32U OSTaskRegGet(INT8U prio, INT8U id, INT8U * perr)
{
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;
#endif
	INT32U value;
	OS_TCB *ptcb;


#if OS_ARG_CHK_EN > 0u
	if (prio >= OS_LOWEST_PRIO) {
		if (prio != OS_PRIO_SELF) {
			*perr = OS_ERR_PRIO_INVALID;
			return (0u);
		}
	}
	if (id >= OS_TASK_REG_TBL_SIZE) {
		*perr = OS_ERR_ID_INVALID;
		return (0u);
	}
#endif
	OS_ENTER_CRITICAL();
	if (prio == OS_PRIO_SELF) {	/* See if need to get register from current task      */
		ptcb = OSTCBCur;
	} else {
		ptcb = OSTCBPrioTbl[prio];
	}
	value = ptcb->OSTCBRegTbl[id];
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
	return (value);
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                 SET THE CURRENT VALUE OF A TASK VARIABLE
*
* Description: This function is called to change the current value of a task register.  Task registers
*              are application specific and can be used to store task specific values such as 'error
*              numbers' (i.e. errno), statistics, etc.  Each task register can hold a 32-bit value.
*
* Arguments  : prio      is the priority of the task you want to set the task register for.  If you
*                        specify OS_PRIO_SELF then the task register of the current task will be obtained.
*
*              id        is the 'id' of the desired task register.  Note that the 'id' must be less
*                        than OS_TASK_REG_TBL_SIZE
*
*              value     is the desired value for the task register.
*
*              perr      is a pointer to a variable that will hold an error code related to this call.
*
*                        OS_ERR_NONE            if the call was successful
*                        OS_ERR_PRIO_INVALID    if you specified an invalid priority
*                        OS_ERR_ID_INVALID      if the 'id' is not between 0 and OS_TASK_REG_TBL_SIZE-1
*
* Returns    : The current value of the task's variable or 0 if an error is detected.
*
* Note(s)    : The maximum number of task variables is 254
*********************************************************************************************************
*/

#if OS_TASK_REG_TBL_SIZE > 0u
void OSTaskRegSet(INT8U prio, INT8U id, INT32U value, INT8U * perr)
{
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;
#endif
	OS_TCB *ptcb;


#if OS_ARG_CHK_EN > 0u
	if (prio >= OS_LOWEST_PRIO) {
		if (prio != OS_PRIO_SELF) {
			*perr = OS_ERR_PRIO_INVALID;
			return;
		}
	}
	if (id >= OS_TASK_REG_TBL_SIZE) {
		*perr = OS_ERR_ID_INVALID;
		return;
	}
#endif
	OS_ENTER_CRITICAL();
	if (prio == OS_PRIO_SELF) {	/* See if need to get register from current task      */
		ptcb = OSTCBCur;
	} else {
		ptcb = OSTCBPrioTbl[prio];
	}
	ptcb->OSTCBRegTbl[id] = value;
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                              CATCH ACCIDENTAL TASK RETURN
*
* Description: This function is called if a task accidentally returns without deleting itself.  In other
*              words, a task should either be an infinite loop or delete itself if it's done.
*              每个任务要么是无限循环，要么任务完成后自行了断删除自己。
*              如果有个任务不是无限循环，任务完成后也没有删除自己，而是执行完毕后返回了
*              本函数处理这种情况
*
* Arguments  : none
*
* Returns    : none
*
* Note(s)    : This function is INTERNAL to uC/OS-II and your application should not call it.
*********************************************************************************************************
*/

void OS_TaskReturn(void)
{
	OSTaskReturnHook(OSTCBCur);	/*用户自定义的处理方式 Call hook to let user decide on what to do        */

#if OS_TASK_DEL_EN > 0u
	(void) OSTaskDel(OS_PRIO_SELF);	/* Delete task if it accidentally returns!           */
#else
	for (;;) {
		OSTimeDly(OS_TICKS_PER_SEC);
	}
#endif
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                        CLEAR TASK STACK
*
* Description: This function is used to clear the stack of a task (i.e. write all zeros)
*              堆栈清零
*
* Arguments  : pbos     is a pointer to the task's bottom of stack.  If the configuration constant
*                       OS_STK_GROWTH is set to 1, the stack is assumed to grow downward (i.e. from high
*                       memory to low memory).  'pbos' will thus point to the lowest (valid) memory
*                       location of the stack.  If OS_STK_GROWTH is set to 0, 'pbos' will point to the
*                       highest memory location of the stack and the stack will grow with increasing
*                       memory locations.  'pbos' MUST point to a valid 'free' data item.
*
*              size     is the number of 'stack elements' to clear.
*
*              opt      contains additional information (or options) about the behavior of the task.  The
*                       LOWER 8-bits are reserved by uC/OS-II while the upper 8 bits can be application
*                       specific.  See OS_TASK_OPT_??? in uCOS-II.H.
*
* Returns    : none
*********************************************************************************************************
*/
#if (OS_TASK_STAT_STK_CHK_EN > 0u) && (OS_TASK_CREATE_EXT_EN > 0u)
void OS_TaskStkClr(OS_STK * pbos, INT32U size, INT16U opt)
{
	if ((opt & OS_TASK_OPT_STK_CHK) != 0x0000u) {	/* See if stack checking has been enabled       */
		if ((opt & OS_TASK_OPT_STK_CLR) != 0x0000u) {	/* See if stack needs to be cleared             */
#if OS_STK_GROWTH == 1u
			while (size > 0u) {	/* Stack grows from HIGH to LOW memory          */
				size--;
				*pbos++ = (OS_STK) 0;	/* Clear from bottom of stack and up!           */
			}
#else
			while (size > 0u) {	/* Stack grows from LOW to HIGH memory          */
				size--;
				*pbos-- = (OS_STK) 0;	/* Clear from bottom of stack and down          */
			}
#endif
		}
	}
}

#endif
