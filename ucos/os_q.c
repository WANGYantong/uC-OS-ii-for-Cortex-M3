/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*                                        MESSAGE QUEUE MANAGEMENT
*
*                              (c) Copyright 1992-2009, Micrium, Weston, FL
*                                           All Rights Reserved
*
* File    : OS_Q.C
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

#if (OS_Q_EN > 0u) && (OS_MAX_QS > 0u)
/*
*********************************************************************************************************
*                                      ACCEPT MESSAGE FROM QUEUE
*
* Description: This function checks the queue to see if a message is available.  Unlike OSQPend(),
*              OSQAccept() does not suspend the calling task if a message is not available.
*              无等待的从消息队列中取得消息
*
* Arguments  : pevent        is a pointer to the event control block
*
*              perr          is a pointer to where an error message will be deposited.  Possible error
*                            messages are:
*
*                            OS_ERR_NONE         The call was successful and your task received a
*                                                message.
*                            OS_ERR_EVENT_TYPE   You didn't pass a pointer to a queue
*                            OS_ERR_PEVENT_NULL  If 'pevent' is a NULL pointer
*                            OS_ERR_Q_EMPTY      The queue did not contain any messages
*
* Returns    : != (void *)0  is the message in the queue if one is available.  The message is removed
*                            from the so the next time OSQAccept() is called, the queue will contain
*                            one less entry.
*              == (void *)0  if you received a NULL pointer message
*                            if the queue is empty or,
*                            if 'pevent' is a NULL pointer or,
*                            if you passed an invalid event type
*
* Note(s)    : As of V2.60, you can now pass NULL pointers through queues.  Because of this, the argument
*              'perr' has been added to the API to tell you about the outcome of the call.
*********************************************************************************************************
*/

#if OS_Q_ACCEPT_EN > 0u
void *OSQAccept(OS_EVENT * pevent, INT8U * perr)
{
	void *pmsg;
	OS_Q *pq;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif

#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                                      */
		*perr = OS_ERR_PEVENT_NULL;
		return ((void *) 0);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*确保ECB类型是消息队列 Validate event block type                          */
		*perr = OS_ERR_EVENT_TYPE;
		return ((void *) 0);
	}
	OS_ENTER_CRITICAL();
	pq = (OS_Q *) pevent->OSEventPtr;	/*指向队列控制块 Point at queue control block                       */
	if (pq->OSQEntries > 0u) {	/*如果消息队列中有消息 See if any messages in the queue                   */
		pmsg = *pq->OSQOut++;	/*取出消息 Yes, extract oldest message from the queue         */
		pq->OSQEntries--;	/*消息计数值减一 Update the number of entries in the queue          */
		if (pq->OSQOut == pq->OSQEnd) {	/*保证循环结构 Wrap OUT pointer if we are at the end of the queue */
			pq->OSQOut = pq->OSQStart;
		}
		*perr = OS_ERR_NONE;
	} else {//如果消息队列中没有消息，则直接退出
		*perr = OS_ERR_Q_EMPTY;
		pmsg = (void *) 0;	/* Queue is empty                                     */
	}
	OS_EXIT_CRITICAL();
	return (pmsg);		/* Return message received (or NULL)                  */
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                        CREATE A MESSAGE QUEUE
*
* Description: This function creates a message queue if free event control blocks are available.
*              创建消息队列
*
* Arguments  : start         is a pointer to the base address of the message queue storage area.  The
*                            storage area MUST be declared as an array of pointers to 'void' as follows
*
*                            void *MessageStorage[size]
*                            消息内存区的基地址，消息内存区是一个指针数组
*
*              size          is the number of elements in the storage area
*                            消息内存区的大小
*
*
* Returns    : != (OS_EVENT *)0  is a pointer to the event control clock (OS_EVENT) associated with the
*                                created queue
*              == (OS_EVENT *)0  if no event control blocks were available or an error was detected
*********************************************************************************************************
*/

OS_EVENT *OSQCreate(void **start, INT16U size)
{
	OS_EVENT *pevent;
	OS_Q *pq;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //才有第三种方式开关中断，需要cpu_sr来保存中断状态
#endif

#ifdef OS_SAFETY_CRITICAL_IEC61508
	if (OSSafetyCriticalStartFlag == OS_TRUE) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

	if (OSIntNesting > 0u) {	/* 确保中断服务子程序不能调用 See if called from ISR ...                         */
		return ((OS_EVENT *) 0);	/* ... can't CREATE from an ISR                       */
	}
	OS_ENTER_CRITICAL();
	pevent = OSEventFreeList;	/*取一个空闲的事件控制块 Get next free event control block                  */
	if (OSEventFreeList != (OS_EVENT *) 0) {	/*检查是否有可用的事件控制块 See if pool of free ECB pool was empty             */
		OSEventFreeList = (OS_EVENT *) OSEventFreeList->OSEventPtr; //调整指针
	}
	OS_EXIT_CRITICAL();
	if (pevent != (OS_EVENT *) 0) {	/*检查获得的事件控制块是否可用 See if we have an event control block              */
		OS_ENTER_CRITICAL();
		pq = OSQFreeList;	/*取一个空闲的队列控制块 Get a free queue control block                     */
		if (pq != (OS_Q *) 0) {	/*如果队列控制块可用 Were we able to get a queue control block ?        */
			OSQFreeList = OSQFreeList->OSQPtr;	/*调整空队列控制块的指针 Yes, Adjust free list pointer to next free */
			OS_EXIT_CRITICAL();
			pq->OSQStart = start;	/*初始化队列     Initialize the queue                 */
			pq->OSQEnd = &start[size];
			pq->OSQIn = start;
			pq->OSQOut = start;
			pq->OSQSize = size;
			pq->OSQEntries = 0u;
			pevent->OSEventType = OS_EVENT_TYPE_Q;  //设置事件控制块类型
			pevent->OSEventCnt = 0u;                //只有事件是信号量时才使用此变量
			pevent->OSEventPtr = pq;                //链接队列控制块到事件控制块
#if OS_EVENT_NAME_EN > 0u
			pevent->OSEventName = (INT8U *) (void *) "?";
#endif
			OS_EventWaitListInit(pevent);	/*初始化等待任务列表 Initalize the wait list              */
		} else {
			pevent->OSEventPtr = (void *) OSEventFreeList;	/* 如果没有可用的队列控制块 No,  Return event control block on error  */
			OSEventFreeList = pevent;       //将时间控制块归还给空闲时间控制块链表
			OS_EXIT_CRITICAL();
			pevent = (OS_EVENT *) 0;
		}
	}
	return (pevent);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                        DELETE A MESSAGE QUEUE
*
* Description: This function deletes a message queue and readies all tasks pending on the queue.
*              删除消息队列
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired
*                            queue.
*                            指向即将删除的消息队列事件指针
*
*              opt           determines delete options as follows:
*                            定义消息队列删除条件的选项:
*                            opt == OS_DEL_NO_PEND   Delete the queue ONLY if no task pending
*                            只能在没有任何任务等待该消息队列的消息时，才可以删除消息队列
*                            opt == OS_DEL_ALWAYS    Deletes the queue even if tasks are waiting.
*                                                    In this case, all the tasks pending will be readied.
*                            不管有没有任务在等待消息队列的消息，都立即删除消息队列。淡出后，所有等待消息的任务立即进入就绪态
*
*              perr          is a pointer to an error code that can contain one of the following values:
*                            OS_ERR_NONE             The call was successful and the queue was deleted
*                            OS_ERR_DEL_ISR          If you tried to delete the queue from an ISR
*                            OS_ERR_INVALID_OPT      An invalid option was specified
*                            OS_ERR_TASK_WAITING     One or more tasks were waiting on the queue
*                            OS_ERR_EVENT_TYPE       If you didn't pass a pointer to a queue
*                            OS_ERR_PEVENT_NULL      If 'pevent' is a NULL pointer.
*
* Returns    : pevent        upon error
*              (OS_EVENT *)0 if the queue was successfully deleted.
*
* Note(s)    : 1) This function must be used with care.  Tasks that would normally expect the presence of
*                 the queue MUST check the return code of OSQPend().
*              2) OSQAccept() callers will not know that the intended queue has been deleted unless
*                 they check 'pevent' to see that it's a NULL pointer.
*              3) This call can potentially disable interrupts for a long time.  The interrupt disable
*                 time is directly proportional to the number of tasks waiting on the queue.
*              4) Because ALL tasks pending on the queue will be readied, you MUST be careful in
*                 applications where the queue is used for mutual exclusion because the resource(s)
*                 will no longer be guarded by the queue.
*              5) If the storage for the message queue was allocated dynamically (i.e. using a malloc()
*                 type call) then your application MUST release the memory storage by call the counterpart
*                 call of the dynamic allocation scheme used.  If the queue storage was created statically
*                 then, the storage can be reused.
*                 应用程序要记得回收分配的内存资源，如果是动态分配的话
*********************************************************************************************************
*/

#if OS_Q_DEL_EN > 0u
OS_EVENT *OSQDel(OS_EVENT * pevent, INT8U opt, INT8U * perr)
{
	BOOLEAN tasks_waiting;
	OS_EVENT *pevent_return;
	OS_Q *pq;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保事件控制块可用 Validate 'pevent'                        */
		*perr = OS_ERR_PEVENT_NULL;
		return (pevent);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*确保ECB类型是队列 Validate event block type                */
		*perr = OS_ERR_EVENT_TYPE;
		return (pevent);
	}
	if (OSIntNesting > 0u) {	/*确保中断中不能调用 See if called from ISR ...               */
		*perr = OS_ERR_DEL_ISR;	/* ... can't DELETE from an ISR             */
		return (pevent);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/* 检查是否有任务在等待消息 See if any tasks waiting on queue        */
		tasks_waiting = OS_TRUE;	/* Yes                                      */
	} else {
		tasks_waiting = OS_FALSE;	/* No                                       */
	}
	switch (opt) {
	case OS_DEL_NO_PEND:	/* 仅在没有任务等待消息时才能删除消息队列 Delete queue only if no task waiting     */
		if (tasks_waiting == OS_FALSE) {//没有任务等待，可以删除
#if OS_EVENT_NAME_EN > 0u
			pevent->OSEventName = (INT8U *) (void *) "?";
#endif
			pq = (OS_Q *) pevent->OSEventPtr;	/*获得消息队列事件的队列控制块 Return OS_Q to free list     */
			pq->OSQPtr = OSQFreeList;           //将队列控制块还给空闲队列指针
			OSQFreeList = pq;                   //调整空闲队列指针
			pevent->OSEventType = OS_EVENT_TYPE_UNUSED; //设置ECB类型为未使用
			pevent->OSEventPtr = OSEventFreeList;	/*将ECB归还给空闲ECB链表 Return Event Control Block to free list  */
			pevent->OSEventCnt = 0u;            //计数值清0
			OSEventFreeList = pevent;	/*调整ECB空闲链表的指针 Get next free event control block        */
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_NONE;
			pevent_return = (OS_EVENT *) 0;	/* Queue has been deleted                   */
		} else {                            //如果有任务在等待消息
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_TASK_WAITING;
			pevent_return = pevent;
		}
		break;

	case OS_DEL_ALWAYS:	/*无论有没有任务等待都删除消息队列 Always delete the queue                  */
		while (pevent->OSEventGrp != 0u) {	/*将所有等待消息的任务都置为就绪态 Ready ALL tasks waiting for queue*/
			(void) OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_Q, OS_STAT_PEND_OK);
		}
#if OS_EVENT_NAME_EN > 0u
		pevent->OSEventName = (INT8U *) (void *) "?";
#endif
		pq = (OS_Q *) pevent->OSEventPtr;	/* Return OS_Q to free list        */
		pq->OSQPtr = OSQFreeList;
		OSQFreeList = pq;
		pevent->OSEventType = OS_EVENT_TYPE_UNUSED;
		pevent->OSEventPtr = OSEventFreeList;	/* Return Event Control Block to free list  */
		pevent->OSEventCnt = 0u;
		OSEventFreeList = pevent;	/* Get next free event control block        */
		OS_EXIT_CRITICAL();
		if (tasks_waiting == OS_TRUE) {	/*如果有任务之前在等待消息，那么现在已经就绪了 Reschedule only if task(s) were waiting  */
			OS_Sched();	/*重新进行任务调度 Find highest priority task ready to run  */
		}
		*perr = OS_ERR_NONE;
		pevent_return = (OS_EVENT *) 0;	/* Queue has been deleted                   */
		break;

	default:
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
*                                             FLUSH QUEUE
*
* Description : This function is used to flush the contents of the message queue.
*               清空消息队列
*
* Arguments   : none
*
* Returns     : OS_ERR_NONE         upon success
*               OS_ERR_EVENT_TYPE   If you didn't pass a pointer to a queue
*               OS_ERR_PEVENT_NULL  If 'pevent' is a NULL pointer
*
* WARNING     : You should use this function with great care because, when to flush the queue, you LOOSE
*               the references to what the queue entries are pointing to and thus, you could cause
*               'memory leaks'.  In other words, the data you are pointing to that's being referenced
*               by the queue entries should, most likely, need to be de-allocated (i.e. freed).
*               使用该函数可能造成内存泄露
*               因为被分配给消息队列指针所指向的那些空间在清空消息队列后可能就成了野指针
*********************************************************************************************************
*/

#if OS_Q_FLUSH_EN > 0u
INT8U OSQFlush(OS_EVENT * pevent)
{
	OS_Q *pq;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式来开关中断，需要cpu_sr来保存中断状态
#endif



#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'  */
		return (OS_ERR_PEVENT_NULL);
	}
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*确保ECB的类型是消息队列 Validate event block type                     */
		return (OS_ERR_EVENT_TYPE);
	}
#endif
	OS_ENTER_CRITICAL();
	pq = (OS_Q *) pevent->OSEventPtr;	/*指向消息队列控制块 Point to queue storage structure              */
	pq->OSQIn = pq->OSQStart;           //指针调整成初始状态
	pq->OSQOut = pq->OSQStart;
	pq->OSQEntries = 0u;                //消息数量清零
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                     PEND ON A QUEUE FOR A MESSAGE
*
* Description: This function waits for a message to be sent to a queue
*              等待消息队列中的消息
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired queue
*
*              timeout       is an optional timeout period (in clock ticks).  If non-zero, your task will
*                            wait for a message to arrive at the queue up to the amount of time
*                            specified by this argument.  If you specify 0, however, your task will wait
*                            forever at the specified queue or, until a message arrives.
*                            等待的超时时间，如果是0标示持续等待
*
*              perr          is a pointer to where an error message will be deposited.  Possible error
*                            messages are:
*
*                            OS_ERR_NONE         The call was successful and your task received a
*                                                message.
*                            OS_ERR_TIMEOUT      A message was not received within the specified 'timeout'.
*                            OS_ERR_PEND_ABORT   The wait on the queue was aborted.
*                            OS_ERR_EVENT_TYPE   You didn't pass a pointer to a queue
*                            OS_ERR_PEVENT_NULL  If 'pevent' is a NULL pointer
*                            OS_ERR_PEND_ISR     If you called this function from an ISR and the result
*                                                would lead to a suspension.
*                            OS_ERR_PEND_LOCKED  If you called this function with the scheduler is locked
*
* Returns    : != (void *)0  is a pointer to the message received
*              == (void *)0  if you received a NULL pointer message or,
*                            if no message was received or,
*                            if 'pevent' is a NULL pointer or,
*                            if you didn't pass a pointer to a queue.
*
* Note(s)    : As of V2.60, this function allows you to receive NULL pointer messages.
*********************************************************************************************************
*/

void *OSQPend(OS_EVENT * pevent, INT32U timeout, INT8U * perr)
{
	void *pmsg;
	OS_Q *pq;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保事件控制块可用 Validate 'pevent'                                  */
		*perr = OS_ERR_PEVENT_NULL;
		return ((void *) 0);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*确保ECB的类型是消息队列 Validate event block type                          */
		*perr = OS_ERR_EVENT_TYPE;
		return ((void *) 0);
	}
	if (OSIntNesting > 0u) {	/*确保中断不能调用 See if called from ISR ...                         */
		*perr = OS_ERR_PEND_ISR;	/* ... can't PEND from an ISR                         */
		return ((void *) 0);
	}
	if (OSLockNesting > 0u) {	/*确保调度不能上锁 See if called with scheduler locked ...            */
		*perr = OS_ERR_PEND_LOCKED;	/* ... can't PEND when locked                         */
		return ((void *) 0);
	}
	OS_ENTER_CRITICAL();
	pq = (OS_Q *) pevent->OSEventPtr;	/*指向队列控制块 Point at queue control block                       */
	if (pq->OSQEntries > 0u) {	/* 队列控制块中是否有消息 See if any messages in the queue                   */
		pmsg = *pq->OSQOut++;	/* 有消息，将消息传递给pmsg Yes, extract oldest message from the queue         */
		pq->OSQEntries--;	/* 队列中消息数减一 Update the number of entries in the queue          */
		if (pq->OSQOut == pq->OSQEnd) {	/*当前指针是否已经到了消息队列的末端 Wrap OUT pointer if we are at the end of the queue */
			pq->OSQOut = pq->OSQStart;  //是，使当前指针指向消息队列缓冲区的首地址，形成循环消息队列
		}
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_NONE;
		return (pmsg);	/* Return message received                            */
	}

    //队列中没有消息
	OSTCBCur->OSTCBStat |= OS_STAT_Q;	/*设置状态标志，等待消息队列 Task will have to pend for a message to be posted  */
	OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK; //设置状态标志，将等待消息队列的任务挂起
	OSTCBCur->OSTCBDly = timeout;	/*等待超时信息保存 Load timeout into TCB                              */
	OS_EventTaskWait(pevent);	/*挂起任务，直到消息队列中有消息或者等待超时 Suspend task until event or timeout occurs         */
	OS_EXIT_CRITICAL();
	OS_Sched();		/* 当前任务被挂起，需要重新调度 Find next highest priority task ready to run       */
	OS_ENTER_CRITICAL();
	switch (OSTCBCur->OSTCBStatPend) {	/*检查任务恢复的原因 See if we timed-out or aborted                */
	case OS_STAT_PEND_OK:	/*由于消息队列中有了消息 Extract message from TCB (Put there by QPost) */
		pmsg = OSTCBCur->OSTCBMsg;  //从消息队列中获取消息
		*perr = OS_ERR_NONE;
		break;

	case OS_STAT_PEND_ABORT:
		pmsg = (void *) 0;
		*perr = OS_ERR_PEND_ABORT;	/*由于挂起被终止放弃了 Indicate that we aborted                      */
		break;

	case OS_STAT_PEND_TO:           //由于等待超时了
	default:
		OS_EventTaskRemove(OSTCBCur, pevent); //将当前任务从消息队列的等待表中删除，前面的两种情况会有相应的函数来将当前任务从等待表中删除
		pmsg = (void *) 0;
		*perr = OS_ERR_TIMEOUT;	/* Indicate that we didn't get event within TO   */
		break;
	}
	OSTCBCur->OSTCBStat = OS_STAT_RDY;	/*设置就绪状态标志 Set   task  status to ready                   */
	OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;	/*清除挂起标志 Clear pend  status                            */
	OSTCBCur->OSTCBEventPtr = (OS_EVENT *) 0;	/*任务已经就绪，不需要等待事件，因此清除TCB中的ECB指针 Clear event pointers                          */
#if (OS_EVENT_MULTI_EN > 0u)
	OSTCBCur->OSTCBEventMultiPtr = (OS_EVENT **) 0;
#endif
	OSTCBCur->OSTCBMsg = (void *) 0;	/* Clear  received message                       */
	OS_EXIT_CRITICAL();
	return (pmsg);		/*返回收到的消息 Return received message                       */
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                      ABORT WAITING ON A MESSAGE QUEUE
*
* Description: This function aborts & readies any tasks currently waiting on a queue.  This function
*              should be used to fault-abort the wait on the queue, rather than to normally signal
*              the queue via OSQPost(), OSQPostFront() or OSQPostOpt().
*              终止等待，所有的等待任务就绪
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired queue.
*
*              opt           determines the type of ABORT performed:
*                            OS_PEND_OPT_NONE         ABORT wait for a single task (HPT) waiting on the
*                                                     queue
*                                                     只终止HPR的等待并使其就绪
*                            OS_PEND_OPT_BROADCAST    ABORT wait for ALL tasks that are  waiting on the
*                                                     queue
*                                                     终止所有任务的等待并使其就绪
*
*              perr          is a pointer to where an error message will be deposited.  Possible error
*                            messages are:
*
*                            OS_ERR_NONE         No tasks were     waiting on the queue.
*                            OS_ERR_PEND_ABORT   At least one task waiting on the queue was readied
*                                                and informed of the aborted wait; check return value
*                                                for the number of tasks whose wait on the queue
*                                                was aborted.
*                            OS_ERR_EVENT_TYPE   If you didn't pass a pointer to a queue.
*                            OS_ERR_PEVENT_NULL  If 'pevent' is a NULL pointer.
*
* Returns    : == 0          if no tasks were waiting on the queue, or upon error.
*              >  0          if one or more tasks waiting on the queue are now readied and informed.
*********************************************************************************************************
*/

#if OS_Q_PEND_ABORT_EN > 0u
INT8U OSQPendAbort(OS_EVENT * pevent, INT8U opt, INT8U * perr)
{
	INT8U nbr_tasks;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB有效 Validate 'pevent'                        */
		*perr = OS_ERR_PEVENT_NULL;
		return (0u);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*确保ECB的类型为消息队列 Validate event block type                */
		*perr = OS_ERR_EVENT_TYPE;
		return (0u);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/*有任务在等待消息 See if any task waiting on queue?        */
		nbr_tasks = 0u;
		switch (opt) {
		case OS_PEND_OPT_BROADCAST:	/*终止所有任务的等待 Do we need to abort ALL waiting tasks?   */
			while (pevent->OSEventGrp != 0u) {	/*将所有的任务置为就绪 Yes, ready ALL tasks waiting on queue    */
				(void) OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_Q, OS_STAT_PEND_ABORT);
				nbr_tasks++;
			}
			break;

		case OS_PEND_OPT_NONE:      //仅终止HPT的等待
		default:	/* HPT任务就绪  No,  ready HPT       waiting on queue    */
			(void) OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_Q, OS_STAT_PEND_ABORT);
			nbr_tasks++;
			break;
		}
		OS_EXIT_CRITICAL();
		OS_Sched();	/*任务调度 Find HPT ready to run                    */
		*perr = OS_ERR_PEND_ABORT;
		return (nbr_tasks);
	}
    //如果没有任务在等待消息
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
	return (0u);		/* No tasks waiting on queue                */
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                        POST MESSAGE TO A QUEUE
*
* Description: This function sends a message to a queue
*              向消息队列发送(FIFO)消息
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired queue
*
*              pmsg          is a pointer to the message to send.
*
* Returns    : OS_ERR_NONE           The call was successful and the message was sent
*              OS_ERR_Q_FULL         If the queue cannot accept any more messages because it is full.
*              OS_ERR_EVENT_TYPE     If you didn't pass a pointer to a queue.
*              OS_ERR_PEVENT_NULL    If 'pevent' is a NULL pointer
*
* Note(s)    : As of V2.60, this function allows you to send NULL pointer messages.
*********************************************************************************************************
*/

#if OS_Q_POST_EN > 0u
INT8U OSQPost(OS_EVENT * pevent, void *pmsg)
{
	OS_Q *pq;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register     */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保事件控制块可用 Validate 'pevent'                            */
		return (OS_ERR_PEVENT_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*确保事件控制块的类型是队列 Validate event block type                    */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/*是否有任务在等待消息 See if any task pending on queue             */
		/* Ready highest priority task waiting on event */
		(void) OS_EventTaskRdy(pevent, pmsg, OS_STAT_Q, OS_STAT_PEND_OK);//若有，使最高优先级任务进入就绪
		OS_EXIT_CRITICAL();
		OS_Sched();	/* 重新调度任务 Find highest priority task ready to run      */
		return (OS_ERR_NONE);
	}
    //没有任务在等待消息
	pq = (OS_Q *) pevent->OSEventPtr;	/*将指针指向消息队列控制块 Point to queue control block                 */
	if (pq->OSQEntries >= pq->OSQSize) {	/*确保消息队列没有满 Make sure queue is not full                  */
		OS_EXIT_CRITICAL();
		return (OS_ERR_Q_FULL);
	}
	*pq->OSQIn++ = pmsg;	/* 将消息插入消息队列 Insert message into queue                    */
	pq->OSQEntries++;	/* 消息队列计数值加一 Update the nbr of entries in the queue       */
	if (pq->OSQIn == pq->OSQEnd) {	/*检查指针是否指向消息队列的末端 Wrap IN ptr if we are at end of queue        */
		pq->OSQIn = pq->OSQStart;   //如果是，调整至真，指向消息队列缓冲区的首地址，构成循环队列
	}
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                   POST MESSAGE TO THE FRONT OF A QUEUE
*
* Description: This function sends a message to a queue but unlike OSQPost(), the message is posted at
*              the front instead of the end of the queue.  Using OSQPostFront() allows you to send
*              'priority' messages.
*              向消息队列发送(LIFO)消息
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired queue
*
*              pmsg          is a pointer to the message to send.
*
* Returns    : OS_ERR_NONE           The call was successful and the message was sent
*              OS_ERR_Q_FULL         If the queue cannot accept any more messages because it is full.
*              OS_ERR_EVENT_TYPE     If you didn't pass a pointer to a queue.
*              OS_ERR_PEVENT_NULL    If 'pevent' is a NULL pointer
*
* Note(s)    : As of V2.60, this function allows you to send NULL pointer messages.
*********************************************************************************************************
*/

#if OS_Q_POST_FRONT_EN > 0u
INT8U OSQPostFront(OS_EVENT * pevent, void *pmsg)
{
	OS_Q *pq;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式来开关中断，需要cpu_sr来保存中断状态
#endif



#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                             */
		return (OS_ERR_PEVENT_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*确保ECB类型是消息队列 Validate event block type                     */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/*是否有任务在等待消息 See if any task pending on queue              */
		/* Ready highest priority task waiting on event  */
		(void) OS_EventTaskRdy(pevent, pmsg, OS_STAT_Q, OS_STAT_PEND_OK);//如果有，HPT就绪
		OS_EXIT_CRITICAL();
		OS_Sched();	/* 任务调度 Find highest priority task ready to run       */
		return (OS_ERR_NONE);
	}
    //如果没有
	pq = (OS_Q *) pevent->OSEventPtr;	/*获取到队列控制块指针 Point to queue control block                  */
	if (pq->OSQEntries >= pq->OSQSize) {	/* 消息队列满了么 Make sure queue is not full                   */
		OS_EXIT_CRITICAL();
		return (OS_ERR_Q_FULL);
	}
	if (pq->OSQOut == pq->OSQStart) {	/*没满的话，检查指针是否指向消息队列的起始端 Wrap OUT ptr if we are at the 1st queue entry */
		pq->OSQOut = pq->OSQEnd;        //若是，将指针指向消息队列的末端，构成循环缓冲区
	}
	pq->OSQOut--;
	*pq->OSQOut = pmsg;	/*消息加入队列 Insert message into queue                     */
	pq->OSQEntries++;	/*消息计数值加一 Update the nbr of entries in the queue        */
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                        POST MESSAGE TO A QUEUE
*
* Description: This function sends a message to a queue.  This call has been added to reduce code size
*              since it can replace both OSQPost() and OSQPostFront().  Also, this function adds the
*              capability to broadcast a message to ALL tasks waiting on the message queue.
*              以可选方式向消息队列发送消息
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired queue
*
*              pmsg          is a pointer to the message to send.
*
*              opt           determines the type of POST performed:
*                            OS_POST_OPT_NONE         POST to a single waiting task
*                                                     (Identical to OSQPost())
*                            OS_POST_OPT_BROADCAST    POST to ALL tasks that are waiting on the queue
*                            OS_POST_OPT_FRONT        POST as LIFO (Simulates OSQPostFront())
*                            OS_POST_OPT_NO_SCHED     Indicates that the scheduler will NOT be invoked
*
* Returns    : OS_ERR_NONE           The call was successful and the message was sent
*              OS_ERR_Q_FULL         If the queue cannot accept any more messages because it is full.
*              OS_ERR_EVENT_TYPE     If you didn't pass a pointer to a queue.
*              OS_ERR_PEVENT_NULL    If 'pevent' is a NULL pointer
*
* Warning    : Interrupts can be disabled for a long time if you do a 'broadcast'.  In fact, the
*              interrupt disable time is proportional to the number of tasks waiting on the queue.
*              采用广播时，可能会关中断很长时间，影响实时性
*********************************************************************************************************
*/

#if OS_Q_POST_OPT_EN > 0u
INT8U OSQPostOpt(OS_EVENT * pevent, void *pmsg, INT8U opt)
{
	OS_Q *pq;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，使用cpu_sr来保存中断状态
#endif



#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保事件控制块可用 Validate 'pevent'                     */
		return (OS_ERR_PEVENT_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*确保ECB类型为任务队列 Validate event block type                     */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0x00u) {	/*是否有任务在等待消息 See if any task pending on queue              */
		if ((opt & OS_POST_OPT_BROADCAST) != 0x00u) {	/*如果是广播方式 Do we need to post msg to ALL waiting tasks ? */
			while (pevent->OSEventGrp != 0u) {	/*将所有等待的任务就绪 Yes, Post to ALL tasks waiting on queue       */
				(void) OS_EventTaskRdy(pevent, pmsg, OS_STAT_Q, OS_STAT_PEND_OK);
			}
		} else {	/*不是广播，仅将HPT就绪 No,  Post to HPT waiting on queue             */
			(void) OS_EventTaskRdy(pevent, pmsg, OS_STAT_Q, OS_STAT_PEND_OK);
		}
		OS_EXIT_CRITICAL();
		if ((opt & OS_POST_OPT_NO_SCHED) == 0u) {	/*参数中是否需要调度 See if scheduler needs to be invoked          */
			OS_Sched();	/* Find highest priority task ready to run       */
		}
		return (OS_ERR_NONE);
	}
    //若没有任务在等待消息
	pq = (OS_Q *) pevent->OSEventPtr;	/* 指向队列控制块Point to queue control block                  */
	if (pq->OSQEntries >= pq->OSQSize) {	/*消息队列是否满了 Make sure queue is not full                   */
		OS_EXIT_CRITICAL();
		return (OS_ERR_Q_FULL);
	}
    //消息队列还没满
	if ((opt & OS_POST_OPT_FRONT) != 0x00u) {	/*采用的是LIFO的方式 Do we post to the FRONT of the queue?         */
		if (pq->OSQOut == pq->OSQStart) {	/*保证缓冲区的循环结构 Yes, Post as LIFO, Wrap OUT pointer if we ... */
			pq->OSQOut = pq->OSQEnd;	/*      ... are at the 1st queue entry           */
		}
		pq->OSQOut--;
		*pq->OSQOut = pmsg;	/*      Insert message into queue                */
	} else {		/*否则是FIFO方式 No,  Post as FIFO                             */
		*pq->OSQIn++ = pmsg;	/*      Insert message into queue                */
		if (pq->OSQIn == pq->OSQEnd) {	/*      Wrap IN ptr if we are at end of queue    */
			pq->OSQIn = pq->OSQStart;
		}
	}
	pq->OSQEntries++;	/* Update the nbr of entries in the queue        */
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                        QUERY A MESSAGE QUEUE
*
* Description: This function obtains information about a message queue.
*              查询消息队列的状态
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired queue
*
*              p_q_data      is a pointer to a structure that will contain information about the message
*                            queue.
*
* Returns    : OS_ERR_NONE         The call was successful and the message was sent
*              OS_ERR_EVENT_TYPE   If you are attempting to obtain data from a non queue.
*              OS_ERR_PEVENT_NULL  If 'pevent'   is a NULL pointer
*              OS_ERR_PDATA_NULL   If 'p_q_data' is a NULL pointer
*********************************************************************************************************
*/

#if OS_Q_QUERY_EN > 0u
INT8U OSQQuery(OS_EVENT * pevent, OS_Q_DATA * p_q_data)
{
	OS_Q *pq;
	INT8U i;
	OS_PRIO *psrc;
	OS_PRIO *pdest;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register     */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                            */
		return (OS_ERR_PEVENT_NULL);
	}
	if (p_q_data == (OS_Q_DATA *) 0) {	/*确保存储的数据结构可用 Validate 'p_q_data'                          */
		return (OS_ERR_PDATA_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*确保ECB的类型是消息队列 Validate event block type                    */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();

    //复制消息队列等待任务列表
	p_q_data->OSEventGrp = pevent->OSEventGrp;	/* Copy message queue wait list                 */
	psrc = &pevent->OSEventTbl[0];
	pdest = &p_q_data->OSEventTbl[0];
	for (i = 0u; i < OS_EVENT_TBL_SIZE; i++) {
		*pdest++ = *psrc++;
	}

    pq = (OS_Q *) pevent->OSEventPtr;   //指向队列控制块
	if (pq->OSQEntries > 0u) {          //如果消息队列中有消息
		p_q_data->OSMsg = *pq->OSQOut;	/*读取该消息 Get next message to return if available      */
	} else {
		p_q_data->OSMsg = (void *) 0;   //没有消息则返回空指针
	}
	p_q_data->OSNMsgs = pq->OSQEntries; //复制消息数量
	p_q_data->OSQSize = pq->OSQSize;    //复制消息队列总容量
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);
}
#endif				/* OS_Q_QUERY_EN                                */

/*$PAGE*/
/*
*********************************************************************************************************
*                                      QUEUE MODULE INITIALIZATION
*
* Description : This function is called by uC/OS-II to initialize the message queue module.  Your
*               application MUST NOT call this function.
*               初始化空消息队列链表
*
* Arguments   :  none
*
* Returns     : none
*
* Note(s)    : This function is INTERNAL to uC/OS-II and your application should not call it.
*********************************************************************************************************
*/

void OS_QInit(void)
{
#if OS_MAX_QS == 1u             //链表中只有一个队列控制块
	OSQFreeList = &OSQTbl[0];	/* Only ONE queue!                                */
	OSQFreeList->OSQPtr = (OS_Q *) 0;
#endif

#if OS_MAX_QS >= 2u             //链表中有多个队列控制块
	INT16U ix;
	INT16U ix_next;
	OS_Q *pq1;
	OS_Q *pq2;



	OS_MemClr((INT8U *) & OSQTbl[0], sizeof(OSQTbl));	/*所有的控制块清零 Clear the queue table                          */
	for (ix = 0u; ix < (OS_MAX_QS - 1u); ix++) {	/*每个队列控制块逐个初始化 Init. list of free QUEUE control blocks        */
		ix_next = ix + 1u;
		pq1 = &OSQTbl[ix];
		pq2 = &OSQTbl[ix_next];
		pq1->OSQPtr = pq2;                          //形成单向链表
	}
	pq1 = &OSQTbl[ix];
	pq1->OSQPtr = (OS_Q *) 0;                       //最后一个控制块指向0
	OSQFreeList = &OSQTbl[0];                       //OSQFreeList指向链表头
#endif
}
#endif				/* OS_Q_EN                                        */
