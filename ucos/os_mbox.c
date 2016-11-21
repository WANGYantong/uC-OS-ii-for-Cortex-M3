/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*                                       MESSAGE MAILBOX MANAGEMENT
*
*                              (c) Copyright 1992-2009, Micrium, Weston, FL
*                                           All Rights Reserved
*
* File    : OS_MBOX.C
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

#if OS_MBOX_EN > 0u
/*
*********************************************************************************************************
*                                     ACCEPT MESSAGE FROM MAILBOX
*
* Description: This function checks the mailbox to see if a message is available.  Unlike OSMboxPend(),
*              OSMboxAccept() does not suspend the calling task if a message is not available.
*              无等待的从消息邮箱中获得消息
*
* Arguments  : pevent        is a pointer to the event control block
*
* Returns    : != (void *)0  is the message in the mailbox if one is available.  The mailbox is cleared
*                            so the next time OSMboxAccept() is called, the mailbox will be empty.
*              == (void *)0  if the mailbox is empty or,
*                            if 'pevent' is a NULL pointer or,
*                            if you didn't pass the proper event pointer.
*********************************************************************************************************
*/

#if OS_MBOX_ACCEPT_EN > 0u
void *OSMboxAccept(OS_EVENT * pevent)
{
	void *pmsg;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register  */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                         */
		return ((void *) 0);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MBOX) {	/*确保事件类型为消息邮箱类型 Validate event block type                 */
		return ((void *) 0);
	}
	OS_ENTER_CRITICAL();
	pmsg = pevent->OSEventPtr;           //不管有无信息，直接取走
	pevent->OSEventPtr = (void *) 0;	/*清空消息邮箱 Clear the mailbox                         */
	OS_EXIT_CRITICAL();
	return (pmsg);		/*返回消息指针，如果没有消息则为空指针 Return the message received (or NULL)     */
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                        CREATE A MESSAGE MAILBOX
*
* Description: This function creates a message mailbox if free event control blocks are available.
*              建立并初始化消息邮箱
*
* Arguments  : pmsg          is a pointer to a message that you wish to deposit in the mailbox.  If
*                            you set this value to the NULL pointer (i.e. (void *)0) then the mailbox
*                            will be considered empty.
*                            指向消息的指针，可以为空，也可以不为空。如果指针不为空，则建立的消息邮箱将含有消息
*
* Returns    : != (OS_EVENT *)0  is a pointer to the event control clock (OS_EVENT) associated with the
*                                created mailbox
*                                指向分配给所建立的任务邮箱的ECB的指针
*              == (OS_EVENT *)0  if no event control blocks were available
*********************************************************************************************************
*/

OS_EVENT *OSMboxCreate(void *pmsg)
{
	OS_EVENT *pevent;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //第三种方式开关中断，需要使用cpu_sr来保存中断状态
#endif

#ifdef OS_SAFETY_CRITICAL_IEC61508
	if (OSSafetyCriticalStartFlag == OS_TRUE) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

	if (OSIntNesting > 0u) {	/*不能在中断中调用 See if called from ISR ...                         */
		return ((OS_EVENT *) 0);	/* ... can't CREATE from an ISR                       */
	}
	OS_ENTER_CRITICAL();
	pevent = OSEventFreeList;	/*从空闲的ECB中取出一个ECB Get next free event control block                  */
	if (OSEventFreeList != (OS_EVENT *) 0) {	/* 如果ECB可用，则使空闲ECB指针指向下一个ECB  See if pool of free ECB pool was empty             */
		OSEventFreeList = (OS_EVENT *) OSEventFreeList->OSEventPtr;
	}
	OS_EXIT_CRITICAL();
	if (pevent != (OS_EVENT *) 0) {             //若ECB可用，则分别设置ECB的事件类型、计数器值、存入消息初始值、事件名称
		pevent->OSEventType = OS_EVENT_TYPE_MBOX;
		pevent->OSEventCnt = 0u;
		pevent->OSEventPtr = pmsg;	/* Deposit message in event control block             */
#if OS_EVENT_NAME_EN > 0u
		pevent->OSEventName = (INT8U *) (void *) "?";
#endif
		OS_EventWaitListInit(pevent);           //初始化ECB等待列表
	}
	return (pevent);	/* Return pointer to event control block              */
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                         DELETE A MAIBOX
*
* Description: This function deletes a mailbox and readies all tasks pending on the mailbox.
*              删除消息邮箱
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired
*                            mailbox.
*                            指向消息邮箱的指针，该指针是消息邮箱建立时，返回给应用程序的指针
*
*              opt           determines delete options as follows:  定义消息邮箱删除条件选项
*                            opt == OS_DEL_NO_PEND   Delete the mailbox ONLY if no task pending
*                                                    只有在没有消息等待该消息邮箱的消息时，才可以删除消息邮箱
*                            opt == OS_DEL_ALWAYS    Deletes the mailbox even if tasks are waiting.
*                                                    In this case, all the tasks pending will be readied.
*                                                    不管有没有任务在等待消息邮箱的消息，立即删除消息邮箱。删除后，
*                                                    所有等待消息的任务立即进入就绪态
*
*              perr          is a pointer to an error code that can contain one of the following values:
*                            OS_ERR_NONE             The call was successful and the mailbox was deleted
*                            OS_ERR_DEL_ISR          If you attempted to delete the mailbox from an ISR
*                            OS_ERR_INVALID_OPT      An invalid option was specified
*                            OS_ERR_TASK_WAITING     One or more tasks were waiting on the mailbox
*                            OS_ERR_EVENT_TYPE       If you didn't pass a pointer to a mailbox
*                            OS_ERR_PEVENT_NULL      If 'pevent' is a NULL pointer.
*
* Returns    : pevent        upon error,这种情况下应该进一步查看出错代码，找到出错原因
*              (OS_EVENT *)0 if the mailbox was successfully deleted.
*
* Note(s)    : 1) This function must be used with care.  Tasks that would normally expect the presence of
*                 the mailbox MUST check the return code of OSMboxPend().
*              2) OSMboxAccept() callers will not know that the intended mailbox has been deleted!
*              3) This call can potentially disable interrupts for a long time.  The interrupt disable
*                 time is directly proportional to the number of tasks waiting on the mailbox.
*              4) Because ALL tasks pending on the mailbox will be readied, you MUST be careful in
*                 applications where the mailbox is used for mutual exclusion because the resource(s)
*                 will no longer be guarded by the mailbox.
*********************************************************************************************************
*/

#if OS_MBOX_DEL_EN > 0u
OS_EVENT *OSMboxDel(OS_EVENT * pevent, INT8U opt, INT8U * perr)
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
	if (pevent == (OS_EVENT *) 0) {	/* 确保指针指向的事件存在 Validate 'pevent'               */
		*perr = OS_ERR_PEVENT_NULL;
		return (pevent);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MBOX) {	/*确保指针指向的是消息邮箱 Validate event block type        */
		*perr = OS_ERR_EVENT_TYPE;
		return (pevent);
	}
	if (OSIntNesting > 0u) {	/*确保调用者不是中断服务程序 See if called from ISR ...               */
		*perr = OS_ERR_DEL_ISR;	/* ... can't DELETE from an ISR             */
		return (pevent);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/*确保是否有任务在等待消息邮箱的消息 See if any tasks waiting on mailbox      */
		tasks_waiting = OS_TRUE;	/* Yes 设置标志变量，以便于函数根据opt的值选择删除方式                        */
	} else {
		tasks_waiting = OS_FALSE;	/* No  没有任务在等待消息                  */
	}
	switch (opt) {

	case OS_DEL_NO_PEND:	/* 只有在没有任务等待消息才可以删除 Delete mailbox only if no task waiting   */
		if (tasks_waiting == OS_FALSE) {    //没有任务在等待消息
#if OS_EVENT_NAME_EN > 0u
			pevent->OSEventName = (INT8U *) (void *) "?";   //改消息邮箱名
#endif
			pevent->OSEventType = OS_EVENT_TYPE_UNUSED;    //设置为未使用状态
			pevent->OSEventPtr = OSEventFreeList;	/*指向空闲ECB链表 Return Event Control Block to free list  */
			pevent->OSEventCnt = 0u;                //计数值清零
			OSEventFreeList = pevent;	/*将ECB归还给空闲ECB链表 Get next free event control block        */
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_NONE;
			pevent_return = (OS_EVENT *) 0;	/*成功删除，返回空指针 Mailbox has been deleted                 */
		} else {                            //有任务在等待消息
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_TASK_WAITING;
			pevent_return = pevent;         //删除不成功，返回ECB指针
		}
		break;

	case OS_DEL_ALWAYS:	/*不管有没有任务在等待消息，都立即删除 Always delete the mailbox                */
		while (pevent->OSEventGrp != 0u) {	/* 将等待消息的所有任务转为就绪态 Ready ALL tasks waiting for mailbox      */
			(void) OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_MBOX, OS_STAT_PEND_OK);
		}
#if OS_EVENT_NAME_EN > 0u
		pevent->OSEventName = (INT8U *) (void *) "?";   //改消息邮箱名
#endif
		pevent->OSEventType = OS_EVENT_TYPE_UNUSED;     //设置为未使用状态
		pevent->OSEventPtr = OSEventFreeList;	/*指向空闲的ECB链表 Return Event Control Block to free list  */
		pevent->OSEventCnt = 0u;                //计数值清零
		OSEventFreeList = pevent;	/* 将ECB归还给空闲ECB链表 Get next free event control block        */
		OS_EXIT_CRITICAL();
		if (tasks_waiting == OS_TRUE) {	/*如果之前是有任务在等待消息，那么现在一定有新任务就绪 Reschedule only if task(s) were waiting  */
			OS_Sched();	/*检查当前任务是否优先级最高，以便进行任务切换 Find highest priority task ready to run  */
		}
		*perr = OS_ERR_NONE;
		pevent_return = (OS_EVENT *) 0;	/* Mailbox has been deleted                 */
		break;

	default:                            //opt参数错误
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
*                                      PEND ON MAILBOX FOR A MESSAGE
*
* Description: This function waits for a message to be sent to a mailbox
*              等待消息邮箱中的消息
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired mailbox
*                            指向接收消息邮箱的指针
*
*              timeout       is an optional timeout period (in clock ticks).  If non-zero, your task will
*                            wait for a message to arrive at the mailbox up to the amount of time
*                            specified by this argument.  If you specify 0, however, your task will wait
*                            forever at the specified mailbox or, until a message arrives.
*                            超时定义，如果为0则表示一直等到接收到消息为止
*
*              perr          is a pointer to where an error message will be deposited.  Possible error
*                            messages are:
*
*                            OS_ERR_NONE         The call was successful and your task received a
*                                                message.
*                            OS_ERR_TIMEOUT      A message was not received within the specified 'timeout'.
*                            OS_ERR_PEND_ABORT   The wait on the mailbox was aborted.
*                            OS_ERR_EVENT_TYPE   Invalid event type
*                            OS_ERR_PEND_ISR     If you called this function from an ISR and the result
*                                                would lead to a suspension.
*                            OS_ERR_PEVENT_NULL  If 'pevent' is a NULL pointer
*                            OS_ERR_PEND_LOCKED  If you called this function when the scheduler is locked
*
* Returns    : != (void *)0  is a pointer to the message received 返回接收到的消息
*              == (void *)0  if no message was received or,
*                            if 'pevent' is a NULL pointer or,
*                            if you didn't pass the proper pointer to the event control block.
*********************************************************************************************************
*/
/*$PAGE*/
void *OSMboxPend(OS_EVENT * pevent, INT32U timeout, INT8U * perr)
{
	void *pmsg;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，使用cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保指针指向的ECB是有效的 Validate 'pevent'                             */
		*perr = OS_ERR_PEVENT_NULL;
		return ((void *) 0);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MBOX) {	/*确保ECB是消息邮箱类型 Validate event block type                     */
		*perr = OS_ERR_EVENT_TYPE;
		return ((void *) 0);
	}
	if (OSIntNesting > 0u) {	/* 确保当前调用者不是中断服务程序 See if called from ISR ...                    */
		*perr = OS_ERR_PEND_ISR;	/* ... can't PEND from an ISR                    */
		return ((void *) 0);
	}
	if (OSLockNesting > 0u) {	/* 确保当前没有调度锁 See if called with scheduler locked ...       */
		*perr = OS_ERR_PEND_LOCKED;	/* ... can't PEND when locked                    */
		return ((void *) 0);
	}
	OS_ENTER_CRITICAL();
	pmsg = pevent->OSEventPtr;  //将消息指针保存到pmsg中
	if (pmsg != (void *) 0) {	/*如果消息邮箱中有消息，取走消息 See if there is already a message             */
		pevent->OSEventPtr = (void *) 0;	/*清空消息邮箱 Clear the mailbox                             */
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_NONE;
		return (pmsg);	/* 返回指向消息的指针 Return the message received (or NULL)         */
	}
    //消息邮箱中没有消息
	OSTCBCur->OSTCBStat |= OS_STAT_MBOX;	/*相应的状态位置位，以挂起调用者 Message not available, task will pend         */
	OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;//设置挂起状态
	OSTCBCur->OSTCBDly = timeout;	/*保存延时时间 Load timeout in TCB                           */
	OS_EventTaskWait(pevent);	/*调用系统函数挂起任务 Suspend task until event or timeout occurs    */
	OS_EXIT_CRITICAL();
	OS_Sched();		/*任务调度 Find next highest priority task ready to run  */
	OS_ENTER_CRITICAL();
    //程序返回后，需要检查返回的原因:是因为收到了消息，还是因为等待超时
	switch (OSTCBCur->OSTCBStatPend) {	/* See if we timed-out or aborted                */
	case OS_STAT_PEND_OK:               //因为收到了消息，将消息保存
		pmsg = OSTCBCur->OSTCBMsg;
		*perr = OS_ERR_NONE;
		break;

	case OS_STAT_PEND_ABORT:            //因为终止
		pmsg = (void *) 0;
		*perr = OS_ERR_PEND_ABORT;	/* Indicate that we aborted                      */
		break;

	case OS_STAT_PEND_TO:               //因为超时
	default:
		OS_EventTaskRemove(OSTCBCur, pevent);   //将任务从事件等待队列中删除
		pmsg = (void *) 0;
		*perr = OS_ERR_TIMEOUT;	/* Indicate that we didn't get event within TO   */
		break;
	}
	OSTCBCur->OSTCBStat = OS_STAT_RDY;	/*设置为就绪状态 Set   task  status to ready                   */
	OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;	/* 清除挂起标志 Clear pend  status                            */
	OSTCBCur->OSTCBEventPtr = (OS_EVENT *) 0;	/* 清楚事件指针 Clear event pointers                          */
#if (OS_EVENT_MULTI_EN > 0u)
	OSTCBCur->OSTCBEventMultiPtr = (OS_EVENT **) 0;
#endif
	OSTCBCur->OSTCBMsg = (void *) 0;	/*清空接收到的消息 Clear  received message                       */
	OS_EXIT_CRITICAL();
	return (pmsg);		/*返回收到的消息 Return received message                       */
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                      ABORT WAITING ON A MESSAGE MAILBOX
*
* Description: This function aborts & readies any tasks currently waiting on a mailbox.  This function
*              should be used to fault-abort the wait on the mailbox, rather than to normally signal
*              the mailbox via OSMboxPost() or OSMboxPostOpt().
*              终止任务等待消息邮箱中的消息
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired mailbox.
*                            消息邮箱的ECB指针
*
*              opt           determines the type of ABORT performed:
*                            终止等待的选项
*                            OS_PEND_OPT_NONE         ABORT wait for a single task (HPT) waiting on the
*                                                     mailbox
*                                                     仅终止等待队列中最高优先级的任务HPT
*                            OS_PEND_OPT_BROADCAST    ABORT wait for ALL tasks that are  waiting on the
*                                                     mailbox
*                                                     终止等待队列中的所有任务
*
*              perr          is a pointer to where an error message will be deposited.  Possible error
*                            messages are:
*
*                            OS_ERR_NONE         No tasks were     waiting on the mailbox.
*                            OS_ERR_PEND_ABORT   At least one task waiting on the mailbox was readied
*                                                and informed of the aborted wait; check return value
*                                                for the number of tasks whose wait on the mailbox
*                                                was aborted.
*                            OS_ERR_EVENT_TYPE   If you didn't pass a pointer to a mailbox.
*                            OS_ERR_PEVENT_NULL  If 'pevent' is a NULL pointer.
*
* Returns    : == 0          if no tasks were waiting on the mailbox, or upon error.
*              >  0          if one or more tasks waiting on the mailbox are now readied and informed.
*********************************************************************************************************
*/

#if OS_MBOX_PEND_ABORT_EN > 0u
INT8U OSMboxPendAbort(OS_EVENT * pevent, INT8U opt, INT8U * perr)
{
	INT8U nbr_tasks;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //第三种方式开关中断，需要cpu_sr来保存中断状态
#endif

#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保指针所指向的ECB有效 Validate 'pevent'                        */
		*perr = OS_ERR_PEVENT_NULL;
		return (0u);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MBOX) {	/* 确保指针所指的ECB是消息邮箱类型  Validate event block type   */
		*perr = OS_ERR_EVENT_TYPE;
		return (0u);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/* 有等待消息邮箱的消息的任务?See if any task waiting on mailbox?      */
		nbr_tasks = 0u;
		switch (opt) {
		case OS_PEND_OPT_BROADCAST:	/*如果是终止所有的任务 Do we need to abort ALL waiting tasks?   */
			while (pevent->OSEventGrp != 0u) {	/*将等待表中的所有任务加入就绪表 Yes, ready ALL tasks waiting on mailbox  */
				(void) OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_MBOX, OS_STAT_PEND_ABORT);
				nbr_tasks++;
			}
			break;

		case OS_PEND_OPT_NONE:      //只终止当前优先级最高的等待任务
		default:	/* No,  ready HPT       waiting on mailbox  */
			(void) OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_MBOX, OS_STAT_PEND_ABORT);
			nbr_tasks++;
			break;
		}
		OS_EXIT_CRITICAL();
		OS_Sched();	/* Find HPT ready to run   任务调度             */
		*perr = OS_ERR_PEND_ABORT;
		return (nbr_tasks); //返回终止的任务个数
	}
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
	return (0u);		/* No tasks waiting on mailbox              */
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                       POST MESSAGE TO A MAILBOX
*
* Description: This function sends a message to a mailbox
*              发送消息到消息邮箱中 
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired mailbox
*
*              pmsg          is a pointer to the message to send.  You MUST NOT send a NULL pointer.
*                            即将发送给任务的消息，不允许传递空指针，以为这意味着消息邮箱为空
*
* Returns    : OS_ERR_NONE          The call was successful and the message was sent
*              OS_ERR_MBOX_FULL     If the mailbox already contains a message.  You can can only send one
*                                   message at a time and thus, the message MUST be consumed before you
*                                   are allowed to send another one.
*              OS_ERR_EVENT_TYPE    If you are attempting to post to a non mailbox.
*              OS_ERR_PEVENT_NULL   If 'pevent' is a NULL pointer
*              OS_ERR_POST_NULL_PTR If you are attempting to post a NULL pointer
*
* Note(s)    : 1) HPT means Highest Priority Task
*********************************************************************************************************
*/

#if OS_MBOX_POST_EN > 0u
INT8U OSMboxPost(OS_EVENT * pevent, void *pmsg)
{
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，所以需要cpu_sr来保存中断状态
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保指针指向的ECB有效 Validate 'pevent'                             */
		return (OS_ERR_PEVENT_NULL);
	}
	if (pmsg == (void *) 0) {	/* 确保发送的不是空指针 Make sure we are not posting a NULL pointer   */
		return (OS_ERR_POST_NULL_PTR);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MBOX) {	/*确保指向的是消息邮箱 Validate event block type                     */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/*如果有任务在等待消息 See if any task pending on mailbox            */
		/*如果有，将该任务置于就绪 Ready HPT waiting on event                    */
		(void) OS_EventTaskRdy(pevent, pmsg, OS_STAT_MBOX, OS_STAT_PEND_OK);
		OS_EXIT_CRITICAL();
		OS_Sched();	/*因为有新任务就绪，所以执行任务调度 Find highest priority task ready to run       */
		return (OS_ERR_NONE);
	}
	if (pevent->OSEventPtr != (void *) 0) {	/*检查消息邮箱是否为满 Make sure mailbox doesn't already have a msg  */
		OS_EXIT_CRITICAL();
		return (OS_ERR_MBOX_FULL);
	}
	pevent->OSEventPtr = pmsg;	/*消息邮箱不为满，将消息指针保存到消息邮箱中 Place message in mailbox                      */
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                       POST MESSAGE TO A MAILBOX
*
* Description: This function sends a message to a mailbox
*              发送消息到消息邮箱中，相比于OSMboxPost()增加了功能选项
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired mailbox
*                            指向即将接收消息的消息邮箱指针
*              pmsg          is a pointer to the message to send.  You MUST NOT send a NULL pointer.
*                            即将发送给任务的消息
*              opt           determines the type of POST performed:
*                            发送消息方式的选项
*                            OS_POST_OPT_NONE         POST to a single waiting task
*                                                     (Identical to OSMboxPost())
*                                                     消息只发送给等待消息的任务中优先级最高的任务
*                            OS_POST_OPT_BROADCAST    POST to ALL tasks that are waiting on the mailbox
*                                                     消息发送给所有等待的任务
*
*                            OS_POST_OPT_NO_SCHED     Indicates that the scheduler will NOT be invoked
*                                                     消息发送但选择关闭调度
*
* Returns    : OS_ERR_NONE          The call was successful and the message was sent
*              OS_ERR_MBOX_FULL     If the mailbox already contains a message.  You can can only send one
*                                   message at a time and thus, the message MUST be consumed before you
*                                   are allowed to send another one.
*              OS_ERR_EVENT_TYPE    If you are attempting to post to a non mailbox.
*              OS_ERR_PEVENT_NULL   If 'pevent' is a NULL pointer
*              OS_ERR_POST_NULL_PTR If you are attempting to post a NULL pointer
*
* Note(s)    : 1) HPT means Highest Priority Task
*
* Warning    : Interrupts can be disabled for a long time if you do a 'broadcast'.  In fact, the
*              interrupt disable time is proportional to the number of tasks waiting on the mailbox.
*********************************************************************************************************
*/

#if OS_MBOX_POST_OPT_EN > 0u
INT8U OSMboxPostOpt(OS_EVENT * pevent, void *pmsg, INT8U opt)
{
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                             */
		return (OS_ERR_PEVENT_NULL);
	}
	if (pmsg == (void *) 0) {	/* 确保发送的消息指针不是空指针 Make sure we are not posting a NULL pointer   */
		return (OS_ERR_POST_NULL_PTR);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MBOX) {	/*确保事件类型是消息邮箱 Validate event block type                     */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/* 是否有任务在等待消息 See if any task pending on mailbox            */
		if ((opt & OS_POST_OPT_BROADCAST) != 0x00u) {	/*如果选项是广播发送 Do we need to post msg to ALL waiting tasks ? */
			while (pevent->OSEventGrp != 0u) {	/*将所有等待消息的任务置于就绪态 Yes, Post to ALL tasks waiting on mailbox     */
				(void) OS_EventTaskRdy(pevent, pmsg, OS_STAT_MBOX, OS_STAT_PEND_OK);
			}
		} else {	/* 如果不是广播发送，将最高优先级的任务置于就绪态 No,  Post to HPT waiting on mbox              */
			(void) OS_EventTaskRdy(pevent, pmsg, OS_STAT_MBOX, OS_STAT_PEND_OK);
		}
		OS_EXIT_CRITICAL();
		if ((opt & OS_POST_OPT_NO_SCHED) == 0u) {	/*如果没有选择关闭调度 See if scheduler needs to be invoked          */
			OS_Sched();	/* Find HPT ready to run  执行任务调度                       */
		}
		return (OS_ERR_NONE);
	}
	if (pevent->OSEventPtr != (void *) 0) {	/*如果没有等待的任务，但是消息邮箱已满 Make sure mailbox doesn't already have a msg  */
		OS_EXIT_CRITICAL();
		return (OS_ERR_MBOX_FULL);          //返回消息邮箱满
	}
	pevent->OSEventPtr = pmsg;	/*消息邮箱为空，并且没有等待任务，将消息保存 Place message in mailbox                      */
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                        QUERY A MESSAGE MAILBOX
*
* Description: This function obtains information about a message mailbox.
*              查询消息邮箱的状态
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired mailbox
*                           待查询的消息邮箱的指针
*
*              p_mbox_data   is a pointer to a structure that will contain information about the message
*                            mailbox.
*                            保存查询的结果。查询的并不是全部ECB的选项，而是一部分内容
*
* Returns    : OS_ERR_NONE         The call was successful and the message was sent
*              OS_ERR_EVENT_TYPE   If you are attempting to obtain data from a non mailbox.
*              OS_ERR_PEVENT_NULL  If 'pevent'      is a NULL pointer
*              OS_ERR_PDATA_NULL   If 'p_mbox_data' is a NULL pointer
*********************************************************************************************************
*/

#if OS_MBOX_QUERY_EN > 0u
INT8U OSMboxQuery(OS_EVENT * pevent, OS_MBOX_DATA * p_mbox_data)
{
	INT8U i;
	OS_PRIO *psrc;
	OS_PRIO *pdest;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //通过方式三来开关中断，需要cpu_sr来保存中断状态
#endif



#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*确保ECB可用 Validate 'pevent'                        */
		return (OS_ERR_PEVENT_NULL);
	}
	if (p_mbox_data == (OS_MBOX_DATA *) 0) {	/*确保有空间保存查询结果 Validate 'p_mbox_data'                   */
		return (OS_ERR_PDATA_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MBOX) {	/*确保事件类型是消息邮箱 Validate event block type                */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	p_mbox_data->OSEventGrp = pevent->OSEventGrp;	/*复制等待任务的组 Copy message mailbox wait list           */
	psrc = &pevent->OSEventTbl[0];                  //消息邮箱中的等待表地址
	pdest = &p_mbox_data->OSEventTbl[0];            //查询结果中的相应地址
	for (i = 0u; i < OS_EVENT_TBL_SIZE; i++) {      //将消息邮箱中的等待表依次复制到查询结果空间中
		*pdest++ = *psrc++;
	}
	p_mbox_data->OSMsg = pevent->OSEventPtr;	/*复制消息邮箱中的消息 Get message from mailbox                 */
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);
}
#endif				/* OS_MBOX_QUERY_EN                         */
#endif				/* OS_MBOX_EN                               */
