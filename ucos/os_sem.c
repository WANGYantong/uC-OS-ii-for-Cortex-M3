/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*                                          SEMAPHORE MANAGEMENT
*
*                              (c) Copyright 1992-2009, Micrium, Weston, FL
*                                           All Rights Reserved
*
* File    : OS_SEM.C
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

#if OS_SEM_EN > 0u
/*$PAGE*/
/*
*********************************************************************************************************
*                                           ACCEPT SEMAPHORE
*
* Description: This function checks the semaphore to see if a resource is available or, if an event
*              occurred.  Unlike OSSemPend(), OSSemAccept() does not suspend the calling task if the
*              resource is not available or the event did not occur.
*              无等待申请信号量
*
* Arguments  : pevent     is a pointer to the event control block
*
* Returns    : >  0       if the resource is available or the event did not occur the semaphore is
*                         decremented to obtain the resource.
*              == 0       if the resource is not available or the event did not occur or,
*                         if 'pevent' is a NULL pointer or,
*                         if you didn't pass a pointer to a semaphore
*********************************************************************************************************
*/

#if OS_SEM_ACCEPT_EN > 0u
INT16U OSSemAccept(OS_EVENT * pevent)
{
	INT16U cnt;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要使用cpu_sr保存中断状态
#endif



#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保事件控制块可用 Validate 'pevent'                             */
		return (0u);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_SEM) {	/*确保事件类型为信号量类型 Validate event block type                     */
		return (0u);
	}
	OS_ENTER_CRITICAL();
	cnt = pevent->OSEventCnt;       //获取信号量的计数值
	if (cnt > 0u) {		/*信号量可以获取，大于0 See if resource is available                  */
		pevent->OSEventCnt--;	/*计数值减一 Yes, decrement semaphore and notify caller    */
	}
	OS_EXIT_CRITICAL();
	return (cnt);		/*返回获取时的信号量计数值 Return semaphore count                        */
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                           CREATE A SEMAPHORE
*
* Description: This function creates a semaphore.
*              创建信号量
*
* Arguments  : cnt           is the initial value for the semaphore.  If the value is 0, no resource is
*                            available (or no event has occurred).  You initialize the semaphore to a
*                            non-zero value to specify how many resources are available (e.g. if you have
*                            10 resources, you would initialize the semaphore to 10).
*
* Returns    : != (void *)0  is a pointer to the event control block (OS_EVENT) associated with the
*                            created semaphore
*              == (void *)0  if no event control blocks were available
*********************************************************************************************************
*/

OS_EVENT *OSSemCreate(INT16U cnt)
{
	OS_EVENT *pevent;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL_IEC61508
	if (OSSafetyCriticalStartFlag == OS_TRUE) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

	if (OSIntNesting > 0u) {	/*中断服务程序中不能创建信号量 See if called from ISR ...               */
		return ((OS_EVENT *) 0);	/* ... can't CREATE from an ISR             */
	}
	OS_ENTER_CRITICAL();
	pevent = OSEventFreeList;	/*从空闲的事件列表中获取一个事件控制块 Get next free event control block        */
	if (OSEventFreeList != (OS_EVENT *) 0) {	/*如果获取的事件控制块可用 See if pool of free ECB pool was empty   */
		OSEventFreeList = (OS_EVENT *) OSEventFreeList->OSEventPtr; //更新空闲事件列表的指针位置
	}
	OS_EXIT_CRITICAL();
	if (pevent != (OS_EVENT *) 0) {	/*确保获取的事件控制块可用 Get an event control block               */
		pevent->OSEventType = OS_EVENT_TYPE_SEM;    //事件类型改为信号量类型
		pevent->OSEventCnt = cnt;	/*设置信号量的初始计数值 Set semaphore value                      */
		pevent->OSEventPtr = (void *) 0;	/*不需要指向消息队列或消息邮箱 Unlink from ECB free list                */
#if OS_EVENT_NAME_EN > 0u
		pevent->OSEventName = (INT8U *) (void *) "?";   //初始化事件名
#endif
		OS_EventWaitListInit(pevent);	/*任务等待标志清0 Initialize to 'nobody waiting' on sem.   */
	}
	return (pevent);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                         DELETE A SEMAPHORE
*
* Description: This function deletes a semaphore and readies all tasks pending on the semaphore.
*              删除信号量，使得等待的任务全部就绪
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired
*                            semaphore.
*
*              opt           determines delete options as follows:
*                            删除方式的参数
*                            opt == OS_DEL_NO_PEND   Delete semaphore ONLY if no task pending
*                                                    只有当没有任务等待时信号量才可以被删除
*                            opt == OS_DEL_ALWAYS    Deletes the semaphore even if tasks are waiting.
*                                                    In this case, all the tasks pending will be readied.
*                                                    不管有没有任务等待，立即删除信号量。所有等待的任务置于就绪态
*
*              perr          is a pointer to an error code that can contain one of the following values:
*                            OS_ERR_NONE             The call was successful and the semaphore was deleted
*                            OS_ERR_DEL_ISR          If you attempted to delete the semaphore from an ISR
*                            OS_ERR_INVALID_OPT      An invalid option was specified
*                            OS_ERR_TASK_WAITING     One or more tasks were waiting on the semaphore
*                            OS_ERR_EVENT_TYPE       If you didn't pass a pointer to a semaphore
*                            OS_ERR_PEVENT_NULL      If 'pevent' is a NULL pointer.
*
* Returns    : pevent        upon error
*              (OS_EVENT *)0 if the semaphore was successfully deleted.
*
* Note(s)    : 1) This function must be used with care.  Tasks that would normally expect the presence of
*                 the semaphore MUST check the return code of OSSemPend().
*              2) OSSemAccept() callers will not know that the intended semaphore has been deleted unless
*                 they check 'pevent' to see that it's a NULL pointer.
*              3) This call can potentially disable interrupts for a long time.  The interrupt disable
*                 time is directly proportional to the number of tasks waiting on the semaphore.
*              4) Because ALL tasks pending on the semaphore will be readied, you MUST be careful in
*                 applications where the semaphore is used for mutual exclusion because the resource(s)
*                 will no longer be guarded by the semaphore.
*********************************************************************************************************
*/

#if OS_SEM_DEL_EN > 0u
OS_EVENT *OSSemDel(OS_EVENT * pevent, INT8U opt, INT8U * perr)
{
	BOOLEAN tasks_waiting;
	OS_EVENT *pevent_return;
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
		return (pevent);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_SEM) {	/*确保ECB是信号量类型 Validate event block type                */
		*perr = OS_ERR_EVENT_TYPE;
		return (pevent);
	}
	if (OSIntNesting > 0u) {	/*确保不能在中断服务程序中被调用 See if called from ISR ...               */
		*perr = OS_ERR_DEL_ISR;	/* ... can't DELETE from an ISR             */
		return (pevent);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/* 是否有任务在等待信号量 See if any tasks waiting on semaphore    */
		tasks_waiting = OS_TRUE;	/* Yes                                      */
	} else {
		tasks_waiting = OS_FALSE;	/* No                                       */
	}
	switch (opt) {
	case OS_DEL_NO_PEND:	/*如果参数是没有等待任务时信号量才能被删除 Delete semaphore only if no task waiting */
		if (tasks_waiting == OS_FALSE) {    //如果此时没有等待的任务
#if OS_EVENT_NAME_EN > 0u
			pevent->OSEventName = (INT8U *) (void *) "?"; //ECB的名字还原
#endif
			pevent->OSEventType = OS_EVENT_TYPE_UNUSED;   //ECB的类型改为未使用类型
			pevent->OSEventPtr = OSEventFreeList;	/*将ECB归还给空闲事件队列 Return Event Control Block to free list  */
			pevent->OSEventCnt = 0u;                //ECB中的信号量计数值清0
			OSEventFreeList = pevent;	/*更新空闲事件队列的队首指针 Get next free event control block        */
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_NONE;            //成功删除
			pevent_return = (OS_EVENT *) 0;	/* Semaphore has been deleted               */
		} else {
			OS_EXIT_CRITICAL();             //如果此时还有任务在等待
			*perr = OS_ERR_TASK_WAITING;    //返回任务等待标志
			pevent_return = pevent;
		}
		break;

	case OS_DEL_ALWAYS:	/*如果参数是无论有无任务等待直接删除信号量 Always delete the semaphore              */
		while (pevent->OSEventGrp != 0u) {	/*就所有等待的任务置为就绪态 Ready ALL tasks waiting for semaphore    */
			(void) OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_SEM, OS_STAT_PEND_OK);
		}
#if OS_EVENT_NAME_EN > 0u
		pevent->OSEventName = (INT8U *) (void *) "?"; //ECB名字还原
#endif
		pevent->OSEventType = OS_EVENT_TYPE_UNUSED;   //ECB类型改为未使用类型
		pevent->OSEventPtr = OSEventFreeList;	/*将ECB归还给空闲事件队列 Return Event Control Block to free list  */
		pevent->OSEventCnt = 0u;                //ECB中的信号量计数值清0
		OSEventFreeList = pevent;	/*更新空闲事件队列的队首指针 Get next free event control block        */
		OS_EXIT_CRITICAL();
		if (tasks_waiting == OS_TRUE) {	/*如果有之前有任务等待 Reschedule only if task(s) were waiting  */
			OS_Sched();	/*调度HPT Find highest priority task ready to run  */
		}
		*perr = OS_ERR_NONE;
		pevent_return = (OS_EVENT *) 0;	/* Semaphore has been deleted               */
		break;

	default:                        //参数输入错误
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
*                                           PEND ON SEMAPHORE
*
* Description: This function waits for a semaphore.
*              请求信号量
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired
*                            semaphore.
*
*              timeout       is an optional timeout period (in clock ticks).  If non-zero, your task will
*                            wait for the resource up to the amount of time specified by this argument.
*                            If you specify 0, however, your task will wait forever at the specified
*                            semaphore or, until the resource becomes available (or the event occurs).
*                            如果参数为0，表示持续等待
*
*              perr          is a pointer to where an error message will be deposited.  Possible error
*                            messages are:
*
*                            OS_ERR_NONE         The call was successful and your task owns the resource
*                                                or, the event you are waiting for occurred.
*                            OS_ERR_TIMEOUT      The semaphore was not received within the specified
*                                                'timeout'.
*                            OS_ERR_PEND_ABORT   The wait on the semaphore was aborted.
*                            OS_ERR_EVENT_TYPE   If you didn't pass a pointer to a semaphore.
*                            OS_ERR_PEND_ISR     If you called this function from an ISR and the result
*                                                would lead to a suspension.
*                            OS_ERR_PEVENT_NULL  If 'pevent' is a NULL pointer.
*                            OS_ERR_PEND_LOCKED  If you called this function when the scheduler is locked
*
* Returns    : none
*********************************************************************************************************
*/
/*$PAGE*/
void OSSemPend(OS_EVENT * pevent, INT32U timeout, INT8U * perr)
{
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                             */
		*perr = OS_ERR_PEVENT_NULL;
		return;
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_SEM) {	/*确保ECB的类型为信号量类型 Validate event block type                     */
		*perr = OS_ERR_EVENT_TYPE;
		return;
	}
	if (OSIntNesting > 0u) {	/*确保不能中断服务函数中调用 See if called from ISR ...                    */
		*perr = OS_ERR_PEND_ISR;	/* ... can't PEND from an ISR                    */
		return;
	}
	if (OSLockNesting > 0u) {	/*确保调度没有上锁 See if called with scheduler locked ...       */
		*perr = OS_ERR_PEND_LOCKED;	/* ... can't PEND when locked                    */
		return;
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventCnt > 0u) {	/*如果此时信号量存在 If sem. is positive, resource available ...   */
		pevent->OSEventCnt--;	/* 获取一个信号量 ... decrement semaphore only if positive.     */
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_NONE;
		return;
	}
	/*否则要等待 Otherwise, must wait until event occurs       */
	OSTCBCur->OSTCBStat |= OS_STAT_SEM;	/*TCB状态置为信号量状态 Resource not available, pend on semaphore     */
	OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;      //TCB置为挂起状态
	OSTCBCur->OSTCBDly = timeout;	/*保存等待时间 Store pend timeout in TCB                     */
	OS_EventTaskWait(pevent);	/*将任务挂起直到超时或者信号量产生 Suspend task until event or timeout occurs    */
	OS_EXIT_CRITICAL();
	OS_Sched();		/*进行任务调度 Find next highest priority task ready         */
	OS_ENTER_CRITICAL();
    //任务重新被执行
	switch (OSTCBCur->OSTCBStatPend) {	/*任务重新被执行的原因:超时还是信号量产生? See if we timed-out or aborted                */
	case OS_STAT_PEND_OK:            //因为信号量产生了，该任务获取到了信号量
		*perr = OS_ERR_NONE;
		break;

	case OS_STAT_PEND_ABORT:
		*perr = OS_ERR_PEND_ABORT;	/*因为调用了终止函数，终止了任务的等待 Indicate that we aborted                      */
		break;

	case OS_STAT_PEND_TO:           //因为等待超时
	default:
		OS_EventTaskRemove(OSTCBCur, pevent);   //将任务从等待列表中删除
		*perr = OS_ERR_TIMEOUT;	/* Indicate that we didn't get event within TO   */
		break;
	}
	OSTCBCur->OSTCBStat = OS_STAT_RDY;	/* 任务状态重新置为就绪态 Set   task  status to ready                   */
	OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;	/*清除挂起标志 Clear pend  status                            */
	OSTCBCur->OSTCBEventPtr = (OS_EVENT *) 0;	/*清除指向等待事件的指针 Clear event pointers                          */
#if (OS_EVENT_MULTI_EN > 0u)
	OSTCBCur->OSTCBEventMultiPtr = (OS_EVENT **) 0;
#endif
	OS_EXIT_CRITICAL();
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                      ABORT WAITING ON A SEMAPHORE
*
* Description: This function aborts & readies any tasks currently waiting on a semaphore.  This function
*              should be used to fault-abort the wait on the semaphore, rather than to normally signal
*              the semaphore via OSSemPost().
*              终止等待并将等待的任务就绪
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired
*                            semaphore.
*
*              opt           determines the type of ABORT performed:
*                            终止的选项
*                            OS_PEND_OPT_NONE         ABORT wait for a single task (HPT) waiting on the
*                                                     semaphore
*                                                     只终止HPT的等待并使其就绪
*                            OS_PEND_OPT_BROADCAST    ABORT wait for ALL tasks that are  waiting on the
*                                                     semaphore
*                                                     终止所有等待任务的等待并使其就绪
*
*              perr          is a pointer to where an error message will be deposited.  Possible error
*                            messages are:
*
*                            OS_ERR_NONE         No tasks were     waiting on the semaphore.
*                            OS_ERR_PEND_ABORT   At least one task waiting on the semaphore was readied
*                                                and informed of the aborted wait; check return value
*                                                for the number of tasks whose wait on the semaphore
*                                                was aborted.
*                            OS_ERR_EVENT_TYPE   If you didn't pass a pointer to a semaphore.
*                            OS_ERR_PEVENT_NULL  If 'pevent' is a NULL pointer.
*
* Returns    : == 0          if no tasks were waiting on the semaphore, or upon error.
*              >  0          if one or more tasks waiting on the semaphore are now readied and informed.
*********************************************************************************************************
*/

#if OS_SEM_PEND_ABORT_EN > 0u
INT8U OSSemPendAbort(OS_EVENT * pevent, INT8U opt, INT8U * perr)
{
	INT8U nbr_tasks;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                             */
		*perr = OS_ERR_PEVENT_NULL;
		return (0u);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_SEM) {	/*确保ECB的类型是信号量类型 Validate event block type                     */
		*perr = OS_ERR_EVENT_TYPE;
		return (0u);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/*如果有任务在等待信号量 See if any task waiting on semaphore?         */
		nbr_tasks = 0u;             //统计恢复就绪的任务计数值
		switch (opt) {
		case OS_PEND_OPT_BROADCAST:	/*如果是终止所有的任务 Do we need to abort ALL waiting tasks?        */
			while (pevent->OSEventGrp != 0u) {	/*将所有的等待任务置为就绪 Yes, ready ALL tasks waiting on semaphore     */
				(void) OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_SEM, OS_STAT_PEND_ABORT);
				nbr_tasks++;        //计数值累加
			}
			break;

		case OS_PEND_OPT_NONE:      //如果只终止HPT的等待
		default:	/* No,  ready HPT       waiting on semaphore     */
			(void) OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_SEM, OS_STAT_PEND_ABORT);  //将HPT置为就绪态
			nbr_tasks++;            //计数值加一
			break;
		}
		OS_EXIT_CRITICAL();
		OS_Sched();	/*由于有任务变为就绪态，需要重新调度 Find HPT ready to run                         */
		*perr = OS_ERR_PEND_ABORT;
		return (nbr_tasks);
	}
    //如果没有任务在等待
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
	return (0u);		/* No tasks waiting on semaphore                 */
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                         POST TO A SEMAPHORE
*
* Description: This function signals a semaphore
*              释放信号量
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired
*                            semaphore.
*
* Returns    : OS_ERR_NONE         The call was successful and the semaphore was signaled.
*              OS_ERR_SEM_OVF      If the semaphore count exceeded its limit.  In other words, you have
*                                  signalled the semaphore more often than you waited on it with either
*                                  OSSemAccept() or OSSemPend().
*              OS_ERR_EVENT_TYPE   If you didn't pass a pointer to a semaphore
*              OS_ERR_PEVENT_NULL  If 'pevent' is a NULL pointer.
*********************************************************************************************************
*/

INT8U OSSemPost(OS_EVENT * pevent)
{
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用  Validate 'pevent'                             */
		return (OS_ERR_PEVENT_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_SEM) {	/*确保ECB的类型是信号量类型 Validate event block type                     */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/*如果有任务在等待 See if any task waiting for semaphore         */
		/* Ready HPT waiting on event                    */
		(void) OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_SEM, OS_STAT_PEND_OK); //将HPT置为就绪
		OS_EXIT_CRITICAL();
		OS_Sched();	/*重新进行任务调度 Find HPT ready to run                         */
		return (OS_ERR_NONE);
	}
    //如果没有任务在等待
	if (pevent->OSEventCnt < 65535u) {	/*确保信号量计数值不会溢出 Make sure semaphore will not overflow         */
		pevent->OSEventCnt++;	/*信号量加一 Increment semaphore count to register event   */
		OS_EXIT_CRITICAL();
		return (OS_ERR_NONE);
	}
	OS_EXIT_CRITICAL();	/* Semaphore value has reached its maximum       */
	return (OS_ERR_SEM_OVF);    //信号量溢出
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                          QUERY A SEMAPHORE
*
* Description: This function obtains information about a semaphore
*              查询信号量的状态
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired
*                            semaphore
*
*              p_sem_data    is a pointer to a structure that will contain information about the
*                            semaphore.
*                            保存查询值得数据结构
*
* Returns    : OS_ERR_NONE         The call was successful and the message was sent
*              OS_ERR_EVENT_TYPE   If you are attempting to obtain data from a non semaphore.
*              OS_ERR_PEVENT_NULL  If 'pevent'     is a NULL pointer.
*              OS_ERR_PDATA_NULL   If 'p_sem_data' is a NULL pointer
*********************************************************************************************************
*/

#if OS_SEM_QUERY_EN > 0u
INT8U OSSemQuery(OS_EVENT * pevent, OS_SEM_DATA * p_sem_data)
{
	INT8U i;
	OS_PRIO *psrc;
	OS_PRIO *pdest;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，使用cpu_sr来保存中断状态
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                        */
		return (OS_ERR_PEVENT_NULL);
	}
	if (p_sem_data == (OS_SEM_DATA *) 0) {	/*确保保存查询结果的数据结构可用 Validate 'p_sem_data'                    */
		return (OS_ERR_PDATA_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_SEM) {	/*确保ECB的类型为信号量类型 Validate event block type                */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	p_sem_data->OSEventGrp = pevent->OSEventGrp;	/*复制信号量的组等待情况 Copy message mailbox wait list           */
	psrc = &pevent->OSEventTbl[0];                  //指向待查询的ECB等待表
	pdest = &p_sem_data->OSEventTbl[0];             //指向保存查询等待表的结果
	for (i = 0u; i < OS_EVENT_TBL_SIZE; i++) {      //将ECB的等待表全部拷贝
		*pdest++ = *psrc++;
	}
	p_sem_data->OSCnt = pevent->OSEventCnt;	/*保存信号量的当前计数值 Get semaphore count                      */
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);
}
#endif				/* OS_SEM_QUERY_EN                          */

/*$PAGE*/
/*
*********************************************************************************************************
*                                              SET SEMAPHORE
*
* Description: This function sets the semaphore count to the value specified as an argument.  Typically,
*              this value would be 0.
*              重置信号量的计数值
*
*              You would typically use this function when a semaphore is used as a signaling mechanism
*              and, you want to reset the count value.
*
* Arguments  : pevent     is a pointer to the event control block
*
*              cnt        is the new value for the semaphore count.  You would pass 0 to reset the
*                         semaphore count.
*
*              perr       is a pointer to an error code returned by the function as follows:
*
*                            OS_ERR_NONE          The call was successful and the semaphore value was set.
*                            OS_ERR_EVENT_TYPE    If you didn't pass a pointer to a semaphore.
*                            OS_ERR_PEVENT_NULL   If 'pevent' is a NULL pointer.
*                            OS_ERR_TASK_WAITING  If tasks are waiting on the semaphore.
*********************************************************************************************************
*/

#if OS_SEM_SET_EN > 0u
void OSSemSet(OS_EVENT * pevent, INT16U cnt, INT8U * perr)
{
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif

#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                             */
		*perr = OS_ERR_PEVENT_NULL;
		return;
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_SEM) {	/*确保ECB的类型为信号量类型 Validate event block type                     */
		*perr = OS_ERR_EVENT_TYPE;
		return;
	}
	OS_ENTER_CRITICAL();
	*perr = OS_ERR_NONE;
	if (pevent->OSEventCnt > 0u) {	/*如果此时信号量计数值大于0，意味着一定没有任务等待 See if semaphore already has a count          */
		pevent->OSEventCnt = cnt;	/*将计数值重置 Yes, set it to the new value specified.       */
	} else {		/*如果信号量计数值为0   No                                            */
		if (pevent->OSEventGrp == 0u) {	/*如果此时没有任务等待      See if task(s) waiting?                  */
			pevent->OSEventCnt = cnt;	/*将计数值重置      No, OK to set the value                  */
		} else {                        //如果此时有任务在等待
			*perr = OS_ERR_TASK_WAITING;//不能重置计数值，表明此时有任务在等待
		}
	}
	OS_EXIT_CRITICAL();
}
#endif

#endif				/* OS_SEM_EN                                     */
