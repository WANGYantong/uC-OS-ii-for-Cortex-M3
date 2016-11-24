/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*                                  MUTUAL EXCLUSION SEMAPHORE MANAGEMENT
*
*                              (c) Copyright 1992-2009, Micrium, Weston, FL
*                                           All Rights Reserved
*
* File    : OS_MUTEX.C
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


#if OS_MUTEX_EN > 0u
/*
*********************************************************************************************************
*                                            LOCAL CONSTANTS
*********************************************************************************************************
*/

#define  OS_MUTEX_KEEP_LOWER_8   ((INT16U)0x00FFu)      //取计数值的低八位
#define  OS_MUTEX_KEEP_UPPER_8   ((INT16U)0xFF00u)      //取计数值的高八位

#define  OS_MUTEX_AVAILABLE      ((INT16U)0x00FFu)      //表明此时互斥信号量是可以获取的

/*
*********************************************************************************************************
*                                            LOCAL CONSTANTS
*********************************************************************************************************
*/

static void OSMutex_RdyAtPrio(OS_TCB * ptcb, INT8U prio);

/*$PAGE*/
/*
*********************************************************************************************************
*                                   ACCEPT MUTUAL EXCLUSION SEMAPHORE
*
* Description: This  function checks the mutual exclusion semaphore to see if a resource is available.
*              Unlike OSMutexPend(), OSMutexAccept() does not suspend the calling task if the resource is
*              not available or the event did not occur.
*              无等待的获取互斥信号量
*
* Arguments  : pevent     is a pointer to the event control block
*
*              perr       is a pointer to an error code which will be returned to your application:
*                            OS_ERR_NONE         if the call was successful.
*                            OS_ERR_EVENT_TYPE   if 'pevent' is not a pointer to a mutex
*                            OS_ERR_PEVENT_NULL  'pevent' is a NULL pointer
*                            OS_ERR_PEND_ISR     if you called this function from an ISR
*                            OS_ERR_PIP_LOWER    If the priority of the task that owns the Mutex is
*                                                HIGHER (i.e. a lower number) than the PIP.  This error
*                                                indicates that you did not set the PIP higher (lower
*                                                number) than ALL the tasks that compete for the Mutex.
*                                                Unfortunately, this is something that could not be
*                                                detected when the Mutex is created because we don't know
*                                                what tasks will be using the Mutex.
*
* Returns    : == OS_TRUE    if the resource is available, the mutual exclusion semaphore is acquired
*              == OS_FALSE   a) if the resource is not available
*                            b) you didn't pass a pointer to a mutual exclusion semaphore
*                            c) you called this function from an ISR
*
* Warning(s) : This function CANNOT be called from an ISR because mutual exclusion semaphores are
*              intended to be used by tasks only.
*              和消息队列、消息邮箱、信号量不同，互斥信号量的无等待获取不能被ISR调用，因为互斥信号量只能被任务使用
*              如果被中断调用了，中断也能进行优先级继承么?谁知道中断会什么时候发生才能释放信号量啊，红红火火恍恍惚惚
********************************************************************************************************
*/

#if OS_MUTEX_ACCEPT_EN > 0u
BOOLEAN OSMutexAccept(OS_EVENT * pevent, INT8U * perr)
{
	INT8U pip;		/* Priority Inheritance Priority (PIP)          */
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register     */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                            */
		*perr = OS_ERR_PEVENT_NULL;
		return (OS_FALSE);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {	/*确保ECB的类型是互斥信号量类型 Validate event block type                    */
		*perr = OS_ERR_EVENT_TYPE;
		return (OS_FALSE);
	}
	if (OSIntNesting > 0u) {	/*确保不能在中断服务程序中 Make sure it's not called from an ISR        */
		*perr = OS_ERR_PEND_ISR;
		return (OS_FALSE);
	}
	OS_ENTER_CRITICAL();	/* Get value (0 or 1) of Mutex                  */
	pip = (INT8U) (pevent->OSEventCnt >> 8u);	/*获取互斥信号量的PIP Get PIP from mutex                           */
	if ((pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8) == OS_MUTEX_AVAILABLE) {//如果计数器的低八位为0xFF,意味着此刻互斥信号量没有被占有
		pevent->OSEventCnt &= OS_MUTEX_KEEP_UPPER_8;	/*清空信号量计数器低八位      Mask off LSByte (Acquire Mutex)         */
		pevent->OSEventCnt |= OSTCBCur->OSTCBPrio;	/*备份当前任务(即将成为信号量拥有者)的优先级      Save current task priority in LSByte    */
		pevent->OSEventPtr = (void *) OSTCBCur;	/*互斥信号量的拥有者指针指向当前任务      Link TCB of task owning Mutex           */
		if (OSTCBCur->OSTCBPrio <= pip) {	/* 如果当前任务的优先级还有高于PIP     PIP 'must' have a SMALLER prio ...      */
			OS_EXIT_CRITICAL();	/*      ... than current task!                  */
			*perr = OS_ERR_PIP_LOWER;       //返回错误代码，意味着PIP设置的有问题
		} else {
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_NONE;            //PIP设置的没毛病
		}
		return (OS_TRUE);                   //获取到了互斥信号量
	}
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
	return (OS_FALSE);                      //当前有任务占有互斥信号量，获取失败
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                  CREATE A MUTUAL EXCLUSION SEMAPHORE
*
* Description: This function creates a mutual exclusion semaphore.
*              创建一个呼出信号量
*
* Arguments  : prio          is the priority to use when accessing the mutual exclusion semaphore.  In
*                            other words, when the semaphore is acquired and a higher priority task
*                            attempts to obtain the semaphore then the priority of the task owning the
*                            semaphore is raised to this priority.  It is assumed that you will specify
*                            a priority that is LOWER in value than ANY of the tasks competing for the
*                            mutex.
*                            优先级继承的优先级，PIP
*
*              perr          is a pointer to an error code which will be returned to your application:
*                               OS_ERR_NONE         if the call was successful.
*                               OS_ERR_CREATE_ISR   if you attempted to create a MUTEX from an ISR
*                               OS_ERR_PRIO_EXIST   if a task at the priority inheritance priority
*                                                   already exist.
*                               OS_ERR_PEVENT_NULL  No more event control blocks available.
*                               OS_ERR_PRIO_INVALID if the priority you specify is higher that the
*                                                   maximum allowed (i.e. > OS_LOWEST_PRIO)
*
* Returns    : != (void *)0  is a pointer to the event control clock (OS_EVENT) associated with the
*                            created mutex.
*              == (void *)0  if an error is detected.
*
* Note(s)    : 1) The LEAST significant 8 bits of '.OSEventCnt' are used to hold the priority number
*                 of the task owning the mutex or 0xFF if no task owns the mutex.
*                 计数值的低八位保存获取到互斥信号量的任务的优先级，如果没有任务获取，此数值为0xFF
*              2) The MOST  significant 8 bits of '.OSEventCnt' are used to hold the priority number
*                 to use to reduce priority inversion.
*                 高八位保存PIP
*********************************************************************************************************
*/

OS_EVENT *OSMutexCreate(INT8U prio, INT8U * perr)
{
	OS_EVENT *pevent;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //如果采用第三种方法开关中断，那么需要cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#ifdef OS_SAFETY_CRITICAL_IEC61508
	if (OSSafetyCriticalStartFlag == OS_TRUE) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (prio >= OS_LOWEST_PRIO) {	/*保证PIP合法 Validate PIP                             */
		*perr = OS_ERR_PRIO_INVALID;
		return ((OS_EVENT *) 0);
	}
#endif
	if (OSIntNesting > 0u) {	/*确保不是在中断服务程序中 See if called from ISR ...               */
		*perr = OS_ERR_CREATE_ISR;	/* ... can't CREATE mutex from an ISR       */
		return ((OS_EVENT *) 0);
	}
	OS_ENTER_CRITICAL();
	if (OSTCBPrioTbl[prio] != (OS_TCB *) 0) {	/*检查PIP是否已经被占用 Mutex priority must not already exist    */
		OS_EXIT_CRITICAL();	/* Task already exist at priority ...       */
		*perr = OS_ERR_PRIO_EXIST;	/*被占用，返回错误代码 ... inheritance priority                 */
		return ((OS_EVENT *) 0);
	}
    //PIP没被占用，可以使用
	OSTCBPrioTbl[prio] = OS_TCB_RESERVED;	/*保留PIP对应的优先级链表 Reserve the table entry                  */
	pevent = OSEventFreeList;	/*从空闲事件队列中取出一块事件控制块 Get next free event control block        */
	if (pevent == (OS_EVENT *) 0) {	/*如果取出的ECB不可用  See if an ECB was available              */
		OSTCBPrioTbl[prio] = (OS_TCB *) 0;	/*释放刚刚占用的PIP对应的优先级链表中的位置 No, Release the table entry              */
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_PEVENT_NULL;	/*返回错误代码 No more event control blocks             */
		return (pevent);
	}
    //取出的ECB可用
	OSEventFreeList = (OS_EVENT *) OSEventFreeList->OSEventPtr;	/*调整空闲事件队列的队首指针 Adjust the free list        */
	OS_EXIT_CRITICAL();
	pevent->OSEventType = OS_EVENT_TYPE_MUTEX;  //将事件类型设置为互斥信号量类型
	pevent->OSEventCnt = (INT16U) ((INT16U) prio << 8u) | OS_MUTEX_AVAILABLE;	/*计数值高八位置为PIP，低八位置为0xFF Resource is avail.  */
	pevent->OSEventPtr = (void *) 0;	/*此时没有任务占用互斥信号量 No task owning the mutex    */
#if OS_EVENT_NAME_EN > 0u
	pevent->OSEventName = (INT8U *) (void *) "?"; //初始化任务名
#endif
	OS_EventWaitListInit(pevent);                 //任务的等待表初始化
	*perr = OS_ERR_NONE;
	return (pevent);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                          DELETE A MUTEX
*
* Description: This function deletes a mutual exclusion semaphore and readies all tasks pending on the it.
*              删除互斥信号量
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired mutex.
*
*              opt           determines delete options as follows:
*                            定义删除条件选项
*                            opt == OS_DEL_NO_PEND   Delete mutex ONLY if no task pending
*                                                    只有在没有任务等待时才删除信号量
*                            opt == OS_DEL_ALWAYS    Deletes the mutex even if tasks are waiting.
*                                                    In this case, all the tasks pending will be readied.
*                                                    不管有没有任务等待，都删除信号量，并将所有的等待任务置为就绪
*
*              perr          is a pointer to an error code that can contain one of the following values:
*                            OS_ERR_NONE             The call was successful and the mutex was deleted
*                            OS_ERR_DEL_ISR          If you attempted to delete the MUTEX from an ISR
*                            OS_ERR_INVALID_OPT      An invalid option was specified
*                            OS_ERR_TASK_WAITING     One or more tasks were waiting on the mutex
*                            OS_ERR_EVENT_TYPE       If you didn't pass a pointer to a mutex
*                            OS_ERR_PEVENT_NULL      If 'pevent' is a NULL pointer.
*
* Returns    : pevent        upon error
*              (OS_EVENT *)0 if the mutex was successfully deleted.
*
* Note(s)    : 1) This function must be used with care.  Tasks that would normally expect the presence of
*                 the mutex MUST check the return code of OSMutexPend().
*
*              2) This call can potentially disable interrupts for a long time.  The interrupt disable
*                 time is directly proportional to the number of tasks waiting on the mutex.
*
*              3) Because ALL tasks pending on the mutex will be readied, you MUST be careful because the
*                 resource(s) will no longer be guarded by the mutex.
*
*              4) IMPORTANT: In the 'OS_DEL_ALWAYS' case, we assume that the owner of the Mutex (if there
*                            is one) is ready-to-run and is thus NOT pending on another kernel object or
*                            has delayed itself.  In other words, if a task owns the mutex being deleted,
*                            that task will be made ready-to-run at its original priority.
*********************************************************************************************************
*/

#if OS_MUTEX_DEL_EN > 0u
OS_EVENT *OSMutexDel(OS_EVENT * pevent, INT8U opt, INT8U * perr)
{
	BOOLEAN tasks_waiting;
	OS_EVENT *pevent_return;
	INT8U pip;		/* Priority inheritance priority            */
	INT8U prio;
	OS_TCB *ptcb;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //使用第三种方式开关中断，需要cpu_sr保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                        */
		*perr = OS_ERR_PEVENT_NULL;
		return (pevent);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {	/*确保ECB的类型是互斥信号量 Validate event block type                */
		*perr = OS_ERR_EVENT_TYPE;
		return (pevent);
	}
	if (OSIntNesting > 0u) {	/*确保不是在中断服务程序中 See if called from ISR ...               */
		*perr = OS_ERR_DEL_ISR;	/* ... can't DELETE from an ISR             */
		return (pevent);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/*是否有任务在等待 See if any tasks waiting on mutex        */
		tasks_waiting = OS_TRUE;	/* Yes                                      */
	} else {
		tasks_waiting = OS_FALSE;	/* No                                       */
	}
	switch (opt) {
	case OS_DEL_NO_PEND:	/*如果参数是只有在没有任务等待的条件下才可以删除互斥信号量 DELETE MUTEX ONLY IF NO TASK WAITING --- */
		if (tasks_waiting == OS_FALSE) {    //此时没有任务等待
#if OS_EVENT_NAME_EN > 0u
			pevent->OSEventName = (INT8U *) (void *) "?";   //初始化事件名
#endif
			pip = (INT8U) (pevent->OSEventCnt >> 8u);       //获取PIP
			OSTCBPrioTbl[pip] = (OS_TCB *) 0;	/*因不再使用而释放PIP Free up the PIP                          */
			pevent->OSEventType = OS_EVENT_TYPE_UNUSED;     //ECB类型改为未使用类型
			pevent->OSEventPtr = OSEventFreeList;	/*将ECB归还空闲事件队列 Return Event Control Block to free list  */
			pevent->OSEventCnt = 0u;                //计数值清零
			OSEventFreeList = pevent;               //调整空闲事件队列的队首指针
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_NONE;
			pevent_return = (OS_EVENT *) 0;	/* Mutex has been deleted                   */
		} else {                            //此时有任务等待，不能删除
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_TASK_WAITING;    //返回错误代码
			pevent_return = pevent;
		}
		break;

	case OS_DEL_ALWAYS:	/*如果参数是不管有没有任务等待，直接删除互斥信号量 ALWAYS DELETE THE MUTEX ---------------- */
		pip = (INT8U) (pevent->OSEventCnt >> 8u);	/*获取PIP Get PIP of mutex          */
		prio = (INT8U) (pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8);	/*获取当前互斥信号量占有者的原来的优先级 Get owner's original prio */
		ptcb = (OS_TCB *) pevent->OSEventPtr;       //获取互斥信号量拥有者的TCB
		if (ptcb != (OS_TCB *) 0) {	/*如果这个拥有者是存在的，即互斥信号量被一个任务占用 See if any task owns the mutex           */
			if (ptcb->OSTCBPrio == pip) {	/*这个拥有者的优先级是否继承了PIP See if original prio was changed         */
				OSMutex_RdyAtPrio(ptcb, prio);	/*如果是，将这个拥有者的优先级还原为它原来的优先级 Yes, Restore the task's original prio    */
			}
		}
		while (pevent->OSEventGrp != 0u) {	/*将所有等待互斥信号量的任务置为就绪 Ready ALL tasks waiting for mutex        */
			(void) OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_MUTEX, OS_STAT_PEND_OK);
		}
#if OS_EVENT_NAME_EN > 0u
		pevent->OSEventName = (INT8U *) (void *) "?";   //互斥信号量的名字还原
#endif
		pip = (INT8U) (pevent->OSEventCnt >> 8u);       //获取到此时的PIP
		OSTCBPrioTbl[pip] = (OS_TCB *) 0;	/*因不再使用而释放PIP Free up the PIP                          */
		pevent->OSEventType = OS_EVENT_TYPE_UNUSED;     //将ECB的类型设置为未使用
		pevent->OSEventPtr = OSEventFreeList;	/*将ECB归还给空闲事件队列 Return Event Control Block to free list  */
		pevent->OSEventCnt = 0u;                //计数值清零
		OSEventFreeList = pevent;	/*调整空闲事件队列的队首指针 Get next free event control block        */
		OS_EXIT_CRITICAL();
		if (tasks_waiting == OS_TRUE) {	/*如果之前有任务等待 Reschedule only if task(s) were waiting  */
			OS_Sched();	/*进行任务调度 Find highest priority task ready to run  */
		}
		*perr = OS_ERR_NONE;
		pevent_return = (OS_EVENT *) 0;	/* Mutex has been deleted                   */
		break;

	default:            //输入参数错误
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_INVALID_OPT;
		pevent_return = pevent;
		break;
	}
	return (pevent_return);
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                  PEND ON MUTUAL EXCLUSION SEMAPHORE
*
* Description: This function waits for a mutual exclusion semaphore.
*              请求获取互斥信号量
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired
*                            mutex.
*
*              timeout       is an optional timeout period (in clock ticks).  If non-zero, your task will
*                            wait for the resource up to the amount of time specified by this argument.
*                            If you specify 0, however, your task will wait forever at the specified
*                            mutex or, until the resource becomes available.
*                            若为0，标示持续等待
*
*              perr          is a pointer to where an error message will be deposited.  Possible error
*                            messages are:
*                               OS_ERR_NONE        The call was successful and your task owns the mutex
*                               OS_ERR_TIMEOUT     The mutex was not available within the specified 'timeout'.
*                               OS_ERR_PEND_ABORT  The wait on the mutex was aborted.
*                               OS_ERR_EVENT_TYPE  If you didn't pass a pointer to a mutex
*                               OS_ERR_PEVENT_NULL 'pevent' is a NULL pointer
*                               OS_ERR_PEND_ISR    If you called this function from an ISR and the result
*                                                  would lead to a suspension.
*                               OS_ERR_PIP_LOWER   If the priority of the task that owns the Mutex is
*                                                  HIGHER (i.e. a lower number) than the PIP.  This error
*                                                  indicates that you did not set the PIP higher (lower
*                                                  number) than ALL the tasks that compete for the Mutex.
*                                                  Unfortunately, this is something that could not be
*                                                  detected when the Mutex is created because we don't know
*                                                  what tasks will be using the Mutex.
*                               OS_ERR_PEND_LOCKED If you called this function when the scheduler is locked
*
* Returns    : none
*
* Note(s)    : 1) The task that owns the Mutex MUST NOT pend on any other event while it owns the mutex.
*
*              2) You MUST NOT change the priority of the task that owns the mutex
*********************************************************************************************************
*/

void OSMutexPend(OS_EVENT * pevent, INT32U timeout, INT8U * perr)
{
	INT8U pip;		/* Priority Inheritance Priority (PIP)      */
	INT8U mprio;		/* Mutex owner priority                     */
	BOOLEAN rdy;		/* Flag indicating task was ready           */
	OS_TCB *ptcb;
	OS_EVENT *pevent2;
	INT8U y;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif


#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                        */
		*perr = OS_ERR_PEVENT_NULL;
		return;
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {	/*确保ECB的类型是互斥信号量 Validate event block type                */
		*perr = OS_ERR_EVENT_TYPE;
		return;
	}
	if (OSIntNesting > 0u) {	/*确保不是在中断服务函数中 See if called from ISR ...               */
		*perr = OS_ERR_PEND_ISR;	/* ... can't PEND from an ISR               */
		return;
	}
	if (OSLockNesting > 0u) {	/*确保调度没有上锁 See if called with scheduler locked ...  */
		*perr = OS_ERR_PEND_LOCKED;	/* ... can't PEND when locked               */
		return;
	}
/*$PAGE*/
	OS_ENTER_CRITICAL();
	pip = (INT8U) (pevent->OSEventCnt >> 8u);	/*获取互斥信号量的PIP Get PIP from mutex                       */
	/* Is Mutex available? 如果信号量可用，则其计数值的低八位应该为0xFF                      */
	if ((INT8U) (pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8) == OS_MUTEX_AVAILABLE) {   //互斥信号量没有被占用
		pevent->OSEventCnt &= OS_MUTEX_KEEP_UPPER_8;	/*将计数器的低八位清零，用来备份该任务当前的优先级 Yes, Acquire the resource                */
		pevent->OSEventCnt |= OSTCBCur->OSTCBPrio;	/*备份该任务的优先级，方便以后恢复      Save priority of owning task        */
		pevent->OSEventPtr = (void *) OSTCBCur;	/*互斥信号量的拥有者是当前任务      Point to owning task's OS_TCB       */
		if (OSTCBCur->OSTCBPrio <= pip) {	/*如果当前任务的优先级要优先于PIP      PIP 'must' have a SMALLER prio ...  */
			OS_EXIT_CRITICAL();	/*      ... than current task!              */
			*perr = OS_ERR_PIP_LOWER;   //说明PIP设置的有问题，返回错误代码
		} else {                        //PIP设置的没毛病
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_NONE;
		}
		return;
	}
    //如果请求的互斥信号量已经被占用
	mprio = (INT8U) (pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8);	/*获取信号量拥有者的原本的优先级 No, Get priority of mutex owner   */
	ptcb = (OS_TCB *) (pevent->OSEventPtr);	/*信号量拥有者的TCB     Point to TCB of mutex owner   */
	if (ptcb->OSTCBPrio > pip) {	/*如果信号量拥有者的优先级要低于PIP     Need to promote prio of owner? */
		if (mprio > OSTCBCur->OSTCBPrio) {//并且信号量拥有者的优先级比当前任务(信号量的请求者)要低
			y = ptcb->OSTCBY;
			if ((OSRdyTbl[y] & ptcb->OSTCBBitX) != 0u) {	/*检查信号量拥有者是否就绪     See if mutex owner is ready   */
				OSRdyTbl[y] &= (OS_PRIO) ~ ptcb->OSTCBBitX;	/*如果就绪，使信号量拥有者脱离就绪，因为要进行优先级继承，变为PIP  Yes, Remove owner from Rdy ... */
				if (OSRdyTbl[y] == 0u) {	/*          ... list at current prio */
					OSRdyGrp &= (OS_PRIO) ~ ptcb->OSTCBBitY;
				}
				rdy = OS_TRUE;                              //设置为就绪状态标志
			} else {                                //如果信号量拥有者不是就绪状态
				pevent2 = ptcb->OSTCBEventPtr;      //获取使得信号量拥有者等待的事件
				if (pevent2 != (OS_EVENT *) 0) {	/*如果该事件存在，将信号量拥有者从等待列表中删除，待会儿用新的优先级阻塞 Remove from event wait list       */
					y = ptcb->OSTCBY;
					pevent2->OSEventTbl[y] &= (OS_PRIO) ~ ptcb->OSTCBBitX;
					if (pevent2->OSEventTbl[y] == 0u) {
						pevent2->OSEventGrp &= (OS_PRIO) ~ ptcb->OSTCBBitY;
					}
				}
				rdy = OS_FALSE;	/* 设置为未就绪状态标志 No                                       */
			}
			ptcb->OSTCBPrio = pip;	/*信号量的拥有者进行优先级的继承，变为PIP Change owner task prio to PIP            */
#if OS_LOWEST_PRIO <= 63u
			ptcb->OSTCBY = (INT8U) (ptcb->OSTCBPrio >> 3u);
			ptcb->OSTCBX = (INT8U) (ptcb->OSTCBPrio & 0x07u);
#else
			ptcb->OSTCBY = (INT8U) ((INT8U) (ptcb->OSTCBPrio >> 4u) & 0xFFu);
			ptcb->OSTCBX = (INT8U) (ptcb->OSTCBPrio & 0x0Fu);
#endif
			ptcb->OSTCBBitY = (OS_PRIO) (1uL << ptcb->OSTCBY);
			ptcb->OSTCBBitX = (OS_PRIO) (1uL << ptcb->OSTCBX);

			if (rdy == OS_TRUE) {	/*如果信号量拥有者在优先级继承之前是就绪的 If task was ready at owner's priority ... */
				OSRdyGrp |= ptcb->OSTCBBitY;	/*使之用新的优先级就绪 ... make it ready at new priority.       */
				OSRdyTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
			} else {                //如果信号量拥有者在优先级继承之前是等待的，则重新使其用新的优先级等待
				pevent2 = ptcb->OSTCBEventPtr;
				if (pevent2 != (OS_EVENT *) 0) {	/* Add to event wait list                   */
					pevent2->OSEventGrp |= ptcb->OSTCBBitY;
					pevent2->OSEventTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
				}
			}
			OSTCBPrioTbl[pip] = ptcb;   //将信号量拥有者的TCB以PIP优先级的身份保存在优先级列表中
		}
	}
	OSTCBCur->OSTCBStat |= OS_STAT_MUTEX;	/*将信号量申请者的状态置为等待互斥信号量 Mutex not available, pend current task        */
	OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;  //将信号量申请者置为挂起状态
	OSTCBCur->OSTCBDly = timeout;	/*保存等待的事件 Store timeout in current task's TCB           */
	OS_EventTaskWait(pevent);	/*将信号量申请者挂起 Suspend task until event or timeout occurs    */
	OS_EXIT_CRITICAL();
	OS_Sched();		/*当前任务被挂起，重新进行任务调度 Find next highest priority task ready         */
    //信号量申请者挂起结束，查找原因:超时?终止?还是获取到了互斥信号量?
	OS_ENTER_CRITICAL();
	switch (OSTCBCur->OSTCBStatPend) {	/* See if we timed-out or aborted                */
	case OS_STAT_PEND_OK:           //因为获取到了互斥信号量
		*perr = OS_ERR_NONE;
		break;

	case OS_STAT_PEND_ABORT:
		*perr = OS_ERR_PEND_ABORT;	/*因为调用了终止等待的函数 Indicate that we aborted getting mutex        */
		break;

	case OS_STAT_PEND_TO:           //因为等待超时
	default:
		OS_EventTaskRemove(OSTCBCur, pevent);   //将信号量申请者从互斥信号量的等待队列中删除
		*perr = OS_ERR_TIMEOUT;	/* Indicate that we didn't get mutex within TO   */
		break;
	}
	OSTCBCur->OSTCBStat = OS_STAT_RDY;	/*将信号量申请者置为就绪态 Set   task  status to ready                   */
	OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;	/*清除挂起标志 Clear pend  status                            */
	OSTCBCur->OSTCBEventPtr = (OS_EVENT *) 0;	/*将等待的事件指针置为空 Clear event pointers                          */
#if (OS_EVENT_MULTI_EN > 0u)
	OSTCBCur->OSTCBEventMultiPtr = (OS_EVENT **) 0;
#endif
	OS_EXIT_CRITICAL();
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                  POST TO A MUTUAL EXCLUSION SEMAPHORE
*
* Description: This function signals a mutual exclusion semaphore
*              释放互斥信号量
*
* Arguments  : pevent              is a pointer to the event control block associated with the desired
*                                  mutex.
*
* Returns    : OS_ERR_NONE             The call was successful and the mutex was signaled.
*              OS_ERR_EVENT_TYPE       If you didn't pass a pointer to a mutex
*              OS_ERR_PEVENT_NULL      'pevent' is a NULL pointer
*              OS_ERR_POST_ISR         Attempted to post from an ISR (not valid for MUTEXes)
*              OS_ERR_NOT_MUTEX_OWNER  The task that did the post is NOT the owner of the MUTEX.
*              OS_ERR_PIP_LOWER        If the priority of the new task that owns the Mutex is
*                                      HIGHER (i.e. a lower number) than the PIP.  This error
*                                      indicates that you did not set the PIP higher (lower
*                                      number) than ALL the tasks that compete for the Mutex.
*                                      Unfortunately, this is something that could not be
*                                      detected when the Mutex is created because we don't know
*                                      what tasks will be using the Mutex.
*********************************************************************************************************
*/

INT8U OSMutexPost(OS_EVENT * pevent)
{
	INT8U pip;		/* Priority inheritance priority                 */
	INT8U prio;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //采用方法三来开关中断，需要cpu_sr来保存中断状态
#endif



	if (OSIntNesting > 0u) {	/*确保不是在中断服务程序中 See if called from ISR ...                    */
		return (OS_ERR_POST_ISR);	/* ... can't POST mutex from an ISR              */
	}
#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                             */
		return (OS_ERR_PEVENT_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {	/*确保ECB的类型是互斥信号量类型 Validate event block type                     */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	pip = (INT8U) (pevent->OSEventCnt >> 8u);	/*获取互斥信号量PIP Get priority inheritance priority of mutex    */
	prio = (INT8U) (pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8);	/*获取互斥信号量拥有者原来的优先级 Get owner's original priority      */
	if (OSTCBCur != (OS_TCB *) pevent->OSEventPtr) {	/*当前任务是否是互斥信号量的拥有者 See if posting task owns the MUTEX            */
		OS_EXIT_CRITICAL();
		return (OS_ERR_NOT_MUTEX_OWNER);                //如果不是返回错误代码，只有互斥信号量的拥有者才可以释放互斥信号量
	}
	if (OSTCBCur->OSTCBPrio == pip) {	/*互斥信号量拥有者的优先级是否是PIP Did we have to raise current task's priority? */
		OSMutex_RdyAtPrio(OSTCBCur, prio);	/*如果是，意味着进行过优先级继承，将拥有者的优先级还原为其原来的优先级 Restore the task's original priority          */
	}
	OSTCBPrioTbl[pip] = OS_TCB_RESERVED;	/*占用PIP优先级列表 Reserve table entry                           */
	if (pevent->OSEventGrp != 0u) {	/*当前有任务在等待互斥信号量么 Any task waiting for the mutex?               */
		/*如果有，将HPT就绪 Yes, Make HPT waiting for mutex ready         */
		prio = OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_MUTEX, OS_STAT_PEND_OK); //将HPT就绪，并获取其优先级
		pevent->OSEventCnt &= OS_MUTEX_KEEP_UPPER_8;	/*互斥信号量计数器低八位清0      Save priority of mutex's new owner       */
		pevent->OSEventCnt |= prio;                     //备份新的信号量拥有者的原来的优先级
		pevent->OSEventPtr = OSTCBPrioTbl[prio];	/*互斥信号量的拥有者指针指向新的拥有者      Link to new mutex owner's OS_TCB         */
		if (prio <= pip) {	/*如果这个新的拥有者的优先级还要优于PIP      PIP 'must' have a SMALLER prio ...       */
			OS_EXIT_CRITICAL();	/*      ... than current task!                   */
			OS_Sched();	/*任务调度      Find highest priority task ready to run  */
			return (OS_ERR_PIP_LOWER);  //返回错误代码，PIP设置的有问题
		} else {            //PIP优先级高于新的拥有者，没毛病
			OS_EXIT_CRITICAL();
			OS_Sched();	/* 任务调度     Find highest priority task ready to run  */
			return (OS_ERR_NONE);
		}
	}
    //如果没有任务在等待互斥信号量
	pevent->OSEventCnt |= OS_MUTEX_AVAILABLE;	/*信号量计数值低八位置为0xFF No,  Mutex is now available                   */
	pevent->OSEventPtr = (void *) 0;           //拥有者指针置为空
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                     QUERY A MUTUAL EXCLUSION SEMAPHORE
*
* Description: This function obtains information about a mutex
*              获取当前互斥信号量的状态
*
* Arguments  : pevent          is a pointer to the event control block associated with the desired mutex
*
*              p_mutex_data    is a pointer to a structure that will contain information about the mutex
*
* Returns    : OS_ERR_NONE          The call was successful and the message was sent
*              OS_ERR_QUERY_ISR     If you called this function from an ISR
*              OS_ERR_PEVENT_NULL   If 'pevent'       is a NULL pointer
*              OS_ERR_PDATA_NULL    If 'p_mutex_data' is a NULL pointer
*              OS_ERR_EVENT_TYPE    If you are attempting to obtain data from a non mutex.
*********************************************************************************************************
*/

#if OS_MUTEX_QUERY_EN > 0u
INT8U OSMutexQuery(OS_EVENT * pevent, OS_MUTEX_DATA * p_mutex_data)
{
	INT8U i;
	OS_PRIO *psrc;
	OS_PRIO *pdest;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //采用方式三开关中断，所以需要cpu_sr保存中断状态
#endif



	if (OSIntNesting > 0u) {	/*确保不在中断服务程序中 See if called from ISR ...               */
		return (OS_ERR_QUERY_ISR);	/* ... can't QUERY mutex from an ISR        */
	}
#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                        */
		return (OS_ERR_PEVENT_NULL);
	}
	if (p_mutex_data == (OS_MUTEX_DATA *) 0) {	/*确保保存查询结果的数据结构可用 Validate 'p_mutex_data'                  */
		return (OS_ERR_PDATA_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {	/*确保信号量的类型是互斥信号量 Validate event block type                */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	p_mutex_data->OSMutexPIP = (INT8U) (pevent->OSEventCnt >> 8u);  //拷贝PIP
	p_mutex_data->OSOwnerPrio = (INT8U) (pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8);//拷贝信号量拥有者原来的优先级
	if (p_mutex_data->OSOwnerPrio == 0xFFu) {   //如果上一步拷贝的结果是0xFF
		p_mutex_data->OSValue = OS_TRUE;        //意味着没有任务占有互斥信号量，该信号量是可用的
	} else {
		p_mutex_data->OSValue = OS_FALSE;       //反之，则是不可用的
	}
	p_mutex_data->OSEventGrp = pevent->OSEventGrp;	/*拷贝信号量中的等待组 Copy wait list                           */
	psrc = &pevent->OSEventTbl[0];                  //信号量中的等待表的地址
	pdest = &p_mutex_data->OSEventTbl[0];           //保存的数据结构中等待表的地址
	for (i = 0u; i < OS_EVENT_TBL_SIZE; i++) {      //将信号量中的等待表的数据依次拷贝到保存的数据结构中
		*pdest++ = *psrc++;
	}
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);
}
#endif				/* OS_MUTEX_QUERY_EN                        */

/*$PAGE*/
/*
*********************************************************************************************************
*                                RESTORE A TASK BACK TO ITS ORIGINAL PRIORITY
*
* Description: This function makes a task ready at the specified priority
*              将ptcb的优先级恢复为prio
*
* Arguments  : ptcb            is a pointer to OS_TCB of the task to make ready
*
*              prio            is the desired priority
*
* Returns    : none
*********************************************************************************************************
*/

static void OSMutex_RdyAtPrio(OS_TCB * ptcb, INT8U prio)
{
	INT8U y;

    //此时ptcb是以PIP的优先级存在于就绪表中的，将ptcb从就绪表中删除
	y = ptcb->OSTCBY;	/* Remove owner from ready list at 'pip'    */
	OSRdyTbl[y] &= (OS_PRIO) ~ ptcb->OSTCBBitX;
	if (OSRdyTbl[y] == 0u) {
		OSRdyGrp &= (OS_PRIO) ~ ptcb->OSTCBBitY;
	}

	ptcb->OSTCBPrio = prio; //ptcb恢复原本的优先级
	OSPrioCur = prio;	/*当前任务的优先级也一并恢复 The current task is now at this priority */

    //更新ptcb内部快速查找任务优先级的变量
#if OS_LOWEST_PRIO <= 63u
	ptcb->OSTCBY = (INT8U) ((INT8U) (prio >> 3u) & 0x07u);
	ptcb->OSTCBX = (INT8U) (prio & 0x07u);
#else
	ptcb->OSTCBY = (INT8U) ((INT8U) (prio >> 4u) & 0x0Fu);
	ptcb->OSTCBX = (INT8U) (prio & 0x0Fu);
#endif
	ptcb->OSTCBBitY = (OS_PRIO) (1uL << ptcb->OSTCBY);
	ptcb->OSTCBBitX = (OS_PRIO) (1uL << ptcb->OSTCBX);

    //将ptcb重新加入就绪表，此时ptcb是以恢复后的优先级存在于就绪表中
	OSRdyGrp |= ptcb->OSTCBBitY;	/* Make task ready at original priority     */
	OSRdyTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
    //将ptcb加入到相应的优先级表中
	OSTCBPrioTbl[prio] = ptcb;

}


#endif				/* OS_MUTEX_EN                              */
