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
* If you plan on using  uC/OS-II  in a commercial product you need to contact Micri�m to properly license
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
*              �޵ȴ��Ĵ���Ϣ������ȡ����Ϣ
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
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif

#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*ȷ��ECB���� Validate 'pevent'                                      */
		*perr = OS_ERR_PEVENT_NULL;
		return ((void *) 0);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*ȷ��ECB��������Ϣ���� Validate event block type                          */
		*perr = OS_ERR_EVENT_TYPE;
		return ((void *) 0);
	}
	OS_ENTER_CRITICAL();
	pq = (OS_Q *) pevent->OSEventPtr;	/*ָ����п��ƿ� Point at queue control block                       */
	if (pq->OSQEntries > 0u) {	/*�����Ϣ����������Ϣ See if any messages in the queue                   */
		pmsg = *pq->OSQOut++;	/*ȡ����Ϣ Yes, extract oldest message from the queue         */
		pq->OSQEntries--;	/*��Ϣ����ֵ��һ Update the number of entries in the queue          */
		if (pq->OSQOut == pq->OSQEnd) {	/*��֤ѭ���ṹ Wrap OUT pointer if we are at the end of the queue */
			pq->OSQOut = pq->OSQStart;
		}
		*perr = OS_ERR_NONE;
	} else {//�����Ϣ������û����Ϣ����ֱ���˳�
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
*              ������Ϣ����
*
* Arguments  : start         is a pointer to the base address of the message queue storage area.  The
*                            storage area MUST be declared as an array of pointers to 'void' as follows
*
*                            void *MessageStorage[size]
*                            ��Ϣ�ڴ����Ļ���ַ����Ϣ�ڴ�����һ��ָ������
*
*              size          is the number of elements in the storage area
*                            ��Ϣ�ڴ����Ĵ�С
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
	OS_CPU_SR cpu_sr = 0u;      //���е����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif

#ifdef OS_SAFETY_CRITICAL_IEC61508
	if (OSSafetyCriticalStartFlag == OS_TRUE) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

	if (OSIntNesting > 0u) {	/* ȷ���жϷ����ӳ����ܵ��� See if called from ISR ...                         */
		return ((OS_EVENT *) 0);	/* ... can't CREATE from an ISR                       */
	}
	OS_ENTER_CRITICAL();
	pevent = OSEventFreeList;	/*ȡһ�����е��¼����ƿ� Get next free event control block                  */
	if (OSEventFreeList != (OS_EVENT *) 0) {	/*����Ƿ��п��õ��¼����ƿ� See if pool of free ECB pool was empty             */
		OSEventFreeList = (OS_EVENT *) OSEventFreeList->OSEventPtr; //����ָ��
	}
	OS_EXIT_CRITICAL();
	if (pevent != (OS_EVENT *) 0) {	/*����õ��¼����ƿ��Ƿ���� See if we have an event control block              */
		OS_ENTER_CRITICAL();
		pq = OSQFreeList;	/*ȡһ�����еĶ��п��ƿ� Get a free queue control block                     */
		if (pq != (OS_Q *) 0) {	/*������п��ƿ���� Were we able to get a queue control block ?        */
			OSQFreeList = OSQFreeList->OSQPtr;	/*�����ն��п��ƿ��ָ�� Yes, Adjust free list pointer to next free */
			OS_EXIT_CRITICAL();
			pq->OSQStart = start;	/*��ʼ������     Initialize the queue                 */
			pq->OSQEnd = &start[size];
			pq->OSQIn = start;
			pq->OSQOut = start;
			pq->OSQSize = size;
			pq->OSQEntries = 0u;
			pevent->OSEventType = OS_EVENT_TYPE_Q;  //�����¼����ƿ�����
			pevent->OSEventCnt = 0u;                //ֻ���¼����ź���ʱ��ʹ�ô˱���
			pevent->OSEventPtr = pq;                //���Ӷ��п��ƿ鵽�¼����ƿ�
#if OS_EVENT_NAME_EN > 0u
			pevent->OSEventName = (INT8U *) (void *) "?";
#endif
			OS_EventWaitListInit(pevent);	/*��ʼ���ȴ������б� Initalize the wait list              */
		} else {
			pevent->OSEventPtr = (void *) OSEventFreeList;	/* ���û�п��õĶ��п��ƿ� No,  Return event control block on error  */
			OSEventFreeList = pevent;       //��ʱ����ƿ�黹������ʱ����ƿ�����
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
*              ɾ����Ϣ����
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired
*                            queue.
*                            ָ�򼴽�ɾ������Ϣ�����¼�ָ��
*
*              opt           determines delete options as follows:
*                            ������Ϣ����ɾ��������ѡ��:
*                            opt == OS_DEL_NO_PEND   Delete the queue ONLY if no task pending
*                            ֻ����û���κ�����ȴ�����Ϣ���е���Ϣʱ���ſ���ɾ����Ϣ����
*                            opt == OS_DEL_ALWAYS    Deletes the queue even if tasks are waiting.
*                                                    In this case, all the tasks pending will be readied.
*                            ������û�������ڵȴ���Ϣ���е���Ϣ��������ɾ����Ϣ���С����������еȴ���Ϣ�����������������̬
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
*                 Ӧ�ó���Ҫ�ǵû��շ�����ڴ���Դ������Ƕ�̬����Ļ�
*********************************************************************************************************
*/

#if OS_Q_DEL_EN > 0u
OS_EVENT *OSQDel(OS_EVENT * pevent, INT8U opt, INT8U * perr)
{
	BOOLEAN tasks_waiting;
	OS_EVENT *pevent_return;
	OS_Q *pq;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*ȷ���¼����ƿ���� Validate 'pevent'                        */
		*perr = OS_ERR_PEVENT_NULL;
		return (pevent);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*ȷ��ECB�����Ƕ��� Validate event block type                */
		*perr = OS_ERR_EVENT_TYPE;
		return (pevent);
	}
	if (OSIntNesting > 0u) {	/*ȷ���ж��в��ܵ��� See if called from ISR ...               */
		*perr = OS_ERR_DEL_ISR;	/* ... can't DELETE from an ISR             */
		return (pevent);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/* ����Ƿ��������ڵȴ���Ϣ See if any tasks waiting on queue        */
		tasks_waiting = OS_TRUE;	/* Yes                                      */
	} else {
		tasks_waiting = OS_FALSE;	/* No                                       */
	}
	switch (opt) {
	case OS_DEL_NO_PEND:	/* ����û������ȴ���Ϣʱ����ɾ����Ϣ���� Delete queue only if no task waiting     */
		if (tasks_waiting == OS_FALSE) {//û������ȴ�������ɾ��
#if OS_EVENT_NAME_EN > 0u
			pevent->OSEventName = (INT8U *) (void *) "?";
#endif
			pq = (OS_Q *) pevent->OSEventPtr;	/*�����Ϣ�����¼��Ķ��п��ƿ� Return OS_Q to free list     */
			pq->OSQPtr = OSQFreeList;           //�����п��ƿ黹�����ж���ָ��
			OSQFreeList = pq;                   //�������ж���ָ��
			pevent->OSEventType = OS_EVENT_TYPE_UNUSED; //����ECB����Ϊδʹ��
			pevent->OSEventPtr = OSEventFreeList;	/*��ECB�黹������ECB���� Return Event Control Block to free list  */
			pevent->OSEventCnt = 0u;            //����ֵ��0
			OSEventFreeList = pevent;	/*����ECB���������ָ�� Get next free event control block        */
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_NONE;
			pevent_return = (OS_EVENT *) 0;	/* Queue has been deleted                   */
		} else {                            //����������ڵȴ���Ϣ
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_TASK_WAITING;
			pevent_return = pevent;
		}
		break;

	case OS_DEL_ALWAYS:	/*������û������ȴ���ɾ����Ϣ���� Always delete the queue                  */
		while (pevent->OSEventGrp != 0u) {	/*�����еȴ���Ϣ��������Ϊ����̬ Ready ALL tasks waiting for queue*/
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
		if (tasks_waiting == OS_TRUE) {	/*���������֮ǰ�ڵȴ���Ϣ����ô�����Ѿ������� Reschedule only if task(s) were waiting  */
			OS_Sched();	/*���½���������� Find highest priority task ready to run  */
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
*               �����Ϣ����
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
*               ʹ�øú�����������ڴ�й¶
*               ��Ϊ���������Ϣ����ָ����ָ�����Щ�ռ��������Ϣ���к���ܾͳ���Ұָ��
*********************************************************************************************************
*/

#if OS_Q_FLUSH_EN > 0u
INT8U OSQFlush(OS_EVENT * pevent)
{
	OS_Q *pq;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�������жϣ���Ҫcpu_sr�������ж�״̬
#endif



#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*ȷ��ECB���� Validate 'pevent'  */
		return (OS_ERR_PEVENT_NULL);
	}
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*ȷ��ECB����������Ϣ���� Validate event block type                     */
		return (OS_ERR_EVENT_TYPE);
	}
#endif
	OS_ENTER_CRITICAL();
	pq = (OS_Q *) pevent->OSEventPtr;	/*ָ����Ϣ���п��ƿ� Point to queue storage structure              */
	pq->OSQIn = pq->OSQStart;           //ָ������ɳ�ʼ״̬
	pq->OSQOut = pq->OSQStart;
	pq->OSQEntries = 0u;                //��Ϣ��������
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
*              �ȴ���Ϣ�����е���Ϣ
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired queue
*
*              timeout       is an optional timeout period (in clock ticks).  If non-zero, your task will
*                            wait for a message to arrive at the queue up to the amount of time
*                            specified by this argument.  If you specify 0, however, your task will wait
*                            forever at the specified queue or, until a message arrives.
*                            �ȴ��ĳ�ʱʱ�䣬�����0��ʾ�����ȴ�
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
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*ȷ���¼����ƿ���� Validate 'pevent'                                  */
		*perr = OS_ERR_PEVENT_NULL;
		return ((void *) 0);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*ȷ��ECB����������Ϣ���� Validate event block type                          */
		*perr = OS_ERR_EVENT_TYPE;
		return ((void *) 0);
	}
	if (OSIntNesting > 0u) {	/*ȷ���жϲ��ܵ��� See if called from ISR ...                         */
		*perr = OS_ERR_PEND_ISR;	/* ... can't PEND from an ISR                         */
		return ((void *) 0);
	}
	if (OSLockNesting > 0u) {	/*ȷ�����Ȳ������� See if called with scheduler locked ...            */
		*perr = OS_ERR_PEND_LOCKED;	/* ... can't PEND when locked                         */
		return ((void *) 0);
	}
	OS_ENTER_CRITICAL();
	pq = (OS_Q *) pevent->OSEventPtr;	/*ָ����п��ƿ� Point at queue control block                       */
	if (pq->OSQEntries > 0u) {	/* ���п��ƿ����Ƿ�����Ϣ See if any messages in the queue                   */
		pmsg = *pq->OSQOut++;	/* ����Ϣ������Ϣ���ݸ�pmsg Yes, extract oldest message from the queue         */
		pq->OSQEntries--;	/* ��������Ϣ����һ Update the number of entries in the queue          */
		if (pq->OSQOut == pq->OSQEnd) {	/*��ǰָ���Ƿ��Ѿ�������Ϣ���е�ĩ�� Wrap OUT pointer if we are at the end of the queue */
			pq->OSQOut = pq->OSQStart;  //�ǣ�ʹ��ǰָ��ָ����Ϣ���л��������׵�ַ���γ�ѭ����Ϣ����
		}
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_NONE;
		return (pmsg);	/* Return message received                            */
	}

    //������û����Ϣ
	OSTCBCur->OSTCBStat |= OS_STAT_Q;	/*����״̬��־���ȴ���Ϣ���� Task will have to pend for a message to be posted  */
	OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK; //����״̬��־�����ȴ���Ϣ���е��������
	OSTCBCur->OSTCBDly = timeout;	/*�ȴ���ʱ��Ϣ���� Load timeout into TCB                              */
	OS_EventTaskWait(pevent);	/*��������ֱ����Ϣ����������Ϣ���ߵȴ���ʱ Suspend task until event or timeout occurs         */
	OS_EXIT_CRITICAL();
	OS_Sched();		/* ��ǰ���񱻹�����Ҫ���µ��� Find next highest priority task ready to run       */
	OS_ENTER_CRITICAL();
	switch (OSTCBCur->OSTCBStatPend) {	/*�������ָ���ԭ�� See if we timed-out or aborted                */
	case OS_STAT_PEND_OK:	/*������Ϣ������������Ϣ Extract message from TCB (Put there by QPost) */
		pmsg = OSTCBCur->OSTCBMsg;  //����Ϣ�����л�ȡ��Ϣ
		*perr = OS_ERR_NONE;
		break;

	case OS_STAT_PEND_ABORT:
		pmsg = (void *) 0;
		*perr = OS_ERR_PEND_ABORT;	/*���ڹ�����ֹ������ Indicate that we aborted                      */
		break;

	case OS_STAT_PEND_TO:           //���ڵȴ���ʱ��
	default:
		OS_EventTaskRemove(OSTCBCur, pevent); //����ǰ�������Ϣ���еĵȴ�����ɾ����ǰ����������������Ӧ�ĺ���������ǰ����ӵȴ�����ɾ��
		pmsg = (void *) 0;
		*perr = OS_ERR_TIMEOUT;	/* Indicate that we didn't get event within TO   */
		break;
	}
	OSTCBCur->OSTCBStat = OS_STAT_RDY;	/*���þ���״̬��־ Set   task  status to ready                   */
	OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;	/*��������־ Clear pend  status                            */
	OSTCBCur->OSTCBEventPtr = (OS_EVENT *) 0;	/*�����Ѿ�����������Ҫ�ȴ��¼���������TCB�е�ECBָ�� Clear event pointers                          */
#if (OS_EVENT_MULTI_EN > 0u)
	OSTCBCur->OSTCBEventMultiPtr = (OS_EVENT **) 0;
#endif
	OSTCBCur->OSTCBMsg = (void *) 0;	/* Clear  received message                       */
	OS_EXIT_CRITICAL();
	return (pmsg);		/*�����յ�����Ϣ Return received message                       */
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                      ABORT WAITING ON A MESSAGE QUEUE
*
* Description: This function aborts & readies any tasks currently waiting on a queue.  This function
*              should be used to fault-abort the wait on the queue, rather than to normally signal
*              the queue via OSQPost(), OSQPostFront() or OSQPostOpt().
*              ��ֹ�ȴ������еĵȴ��������
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired queue.
*
*              opt           determines the type of ABORT performed:
*                            OS_PEND_OPT_NONE         ABORT wait for a single task (HPT) waiting on the
*                                                     queue
*                                                     ֻ��ֹHPR�ĵȴ���ʹ�����
*                            OS_PEND_OPT_BROADCAST    ABORT wait for ALL tasks that are  waiting on the
*                                                     queue
*                                                     ��ֹ��������ĵȴ���ʹ�����
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
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*ȷ��ECB��Ч Validate 'pevent'                        */
		*perr = OS_ERR_PEVENT_NULL;
		return (0u);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*ȷ��ECB������Ϊ��Ϣ���� Validate event block type                */
		*perr = OS_ERR_EVENT_TYPE;
		return (0u);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/*�������ڵȴ���Ϣ See if any task waiting on queue?        */
		nbr_tasks = 0u;
		switch (opt) {
		case OS_PEND_OPT_BROADCAST:	/*��ֹ��������ĵȴ� Do we need to abort ALL waiting tasks?   */
			while (pevent->OSEventGrp != 0u) {	/*�����е�������Ϊ���� Yes, ready ALL tasks waiting on queue    */
				(void) OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_Q, OS_STAT_PEND_ABORT);
				nbr_tasks++;
			}
			break;

		case OS_PEND_OPT_NONE:      //����ֹHPT�ĵȴ�
		default:	/* HPT�������  No,  ready HPT       waiting on queue    */
			(void) OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_Q, OS_STAT_PEND_ABORT);
			nbr_tasks++;
			break;
		}
		OS_EXIT_CRITICAL();
		OS_Sched();	/*������� Find HPT ready to run                    */
		*perr = OS_ERR_PEND_ABORT;
		return (nbr_tasks);
	}
    //���û�������ڵȴ���Ϣ
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
*              ����Ϣ���з���(FIFO)��Ϣ
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
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*ȷ���¼����ƿ���� Validate 'pevent'                            */
		return (OS_ERR_PEVENT_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*ȷ���¼����ƿ�������Ƕ��� Validate event block type                    */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/*�Ƿ��������ڵȴ���Ϣ See if any task pending on queue             */
		/* Ready highest priority task waiting on event */
		(void) OS_EventTaskRdy(pevent, pmsg, OS_STAT_Q, OS_STAT_PEND_OK);//���У�ʹ������ȼ�����������
		OS_EXIT_CRITICAL();
		OS_Sched();	/* ���µ������� Find highest priority task ready to run      */
		return (OS_ERR_NONE);
	}
    //û�������ڵȴ���Ϣ
	pq = (OS_Q *) pevent->OSEventPtr;	/*��ָ��ָ����Ϣ���п��ƿ� Point to queue control block                 */
	if (pq->OSQEntries >= pq->OSQSize) {	/*ȷ����Ϣ����û���� Make sure queue is not full                  */
		OS_EXIT_CRITICAL();
		return (OS_ERR_Q_FULL);
	}
	*pq->OSQIn++ = pmsg;	/* ����Ϣ������Ϣ���� Insert message into queue                    */
	pq->OSQEntries++;	/* ��Ϣ���м���ֵ��һ Update the nbr of entries in the queue       */
	if (pq->OSQIn == pq->OSQEnd) {	/*���ָ���Ƿ�ָ����Ϣ���е�ĩ�� Wrap IN ptr if we are at end of queue        */
		pq->OSQIn = pq->OSQStart;   //����ǣ��������棬ָ����Ϣ���л��������׵�ַ������ѭ������
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
*              ����Ϣ���з���(LIFO)��Ϣ
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
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�������жϣ���Ҫcpu_sr�������ж�״̬
#endif



#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*ȷ��ECB���� Validate 'pevent'                             */
		return (OS_ERR_PEVENT_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*ȷ��ECB��������Ϣ���� Validate event block type                     */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/*�Ƿ��������ڵȴ���Ϣ See if any task pending on queue              */
		/* Ready highest priority task waiting on event  */
		(void) OS_EventTaskRdy(pevent, pmsg, OS_STAT_Q, OS_STAT_PEND_OK);//����У�HPT����
		OS_EXIT_CRITICAL();
		OS_Sched();	/* ������� Find highest priority task ready to run       */
		return (OS_ERR_NONE);
	}
    //���û��
	pq = (OS_Q *) pevent->OSEventPtr;	/*��ȡ�����п��ƿ�ָ�� Point to queue control block                  */
	if (pq->OSQEntries >= pq->OSQSize) {	/* ��Ϣ��������ô Make sure queue is not full                   */
		OS_EXIT_CRITICAL();
		return (OS_ERR_Q_FULL);
	}
	if (pq->OSQOut == pq->OSQStart) {	/*û���Ļ������ָ���Ƿ�ָ����Ϣ���е���ʼ�� Wrap OUT ptr if we are at the 1st queue entry */
		pq->OSQOut = pq->OSQEnd;        //���ǣ���ָ��ָ����Ϣ���е�ĩ�ˣ�����ѭ��������
	}
	pq->OSQOut--;
	*pq->OSQOut = pmsg;	/*��Ϣ������� Insert message into queue                     */
	pq->OSQEntries++;	/*��Ϣ����ֵ��һ Update the nbr of entries in the queue        */
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
*              �Կ�ѡ��ʽ����Ϣ���з�����Ϣ
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
*              ���ù㲥ʱ�����ܻ���жϺܳ�ʱ�䣬Ӱ��ʵʱ��
*********************************************************************************************************
*/

#if OS_Q_POST_OPT_EN > 0u
INT8U OSQPostOpt(OS_EVENT * pevent, void *pmsg, INT8U opt)
{
	OS_Q *pq;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ�ʹ��cpu_sr�������ж�״̬
#endif



#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*ȷ���¼����ƿ���� Validate 'pevent'                     */
		return (OS_ERR_PEVENT_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*ȷ��ECB����Ϊ������� Validate event block type                     */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0x00u) {	/*�Ƿ��������ڵȴ���Ϣ See if any task pending on queue              */
		if ((opt & OS_POST_OPT_BROADCAST) != 0x00u) {	/*����ǹ㲥��ʽ Do we need to post msg to ALL waiting tasks ? */
			while (pevent->OSEventGrp != 0u) {	/*�����еȴ���������� Yes, Post to ALL tasks waiting on queue       */
				(void) OS_EventTaskRdy(pevent, pmsg, OS_STAT_Q, OS_STAT_PEND_OK);
			}
		} else {	/*���ǹ㲥������HPT���� No,  Post to HPT waiting on queue             */
			(void) OS_EventTaskRdy(pevent, pmsg, OS_STAT_Q, OS_STAT_PEND_OK);
		}
		OS_EXIT_CRITICAL();
		if ((opt & OS_POST_OPT_NO_SCHED) == 0u) {	/*�������Ƿ���Ҫ���� See if scheduler needs to be invoked          */
			OS_Sched();	/* Find highest priority task ready to run       */
		}
		return (OS_ERR_NONE);
	}
    //��û�������ڵȴ���Ϣ
	pq = (OS_Q *) pevent->OSEventPtr;	/* ָ����п��ƿ�Point to queue control block                  */
	if (pq->OSQEntries >= pq->OSQSize) {	/*��Ϣ�����Ƿ����� Make sure queue is not full                   */
		OS_EXIT_CRITICAL();
		return (OS_ERR_Q_FULL);
	}
    //��Ϣ���л�û��
	if ((opt & OS_POST_OPT_FRONT) != 0x00u) {	/*���õ���LIFO�ķ�ʽ Do we post to the FRONT of the queue?         */
		if (pq->OSQOut == pq->OSQStart) {	/*��֤��������ѭ���ṹ Yes, Post as LIFO, Wrap OUT pointer if we ... */
			pq->OSQOut = pq->OSQEnd;	/*      ... are at the 1st queue entry           */
		}
		pq->OSQOut--;
		*pq->OSQOut = pmsg;	/*      Insert message into queue                */
	} else {		/*������FIFO��ʽ No,  Post as FIFO                             */
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
*              ��ѯ��Ϣ���е�״̬
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
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif



#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*ȷ��ECB���� Validate 'pevent'                            */
		return (OS_ERR_PEVENT_NULL);
	}
	if (p_q_data == (OS_Q_DATA *) 0) {	/*ȷ���洢�����ݽṹ���� Validate 'p_q_data'                          */
		return (OS_ERR_PDATA_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_Q) {	/*ȷ��ECB����������Ϣ���� Validate event block type                    */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();

    //������Ϣ���еȴ������б�
	p_q_data->OSEventGrp = pevent->OSEventGrp;	/* Copy message queue wait list                 */
	psrc = &pevent->OSEventTbl[0];
	pdest = &p_q_data->OSEventTbl[0];
	for (i = 0u; i < OS_EVENT_TBL_SIZE; i++) {
		*pdest++ = *psrc++;
	}

    pq = (OS_Q *) pevent->OSEventPtr;   //ָ����п��ƿ�
	if (pq->OSQEntries > 0u) {          //�����Ϣ����������Ϣ
		p_q_data->OSMsg = *pq->OSQOut;	/*��ȡ����Ϣ Get next message to return if available      */
	} else {
		p_q_data->OSMsg = (void *) 0;   //û����Ϣ�򷵻ؿ�ָ��
	}
	p_q_data->OSNMsgs = pq->OSQEntries; //������Ϣ����
	p_q_data->OSQSize = pq->OSQSize;    //������Ϣ����������
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
*               ��ʼ������Ϣ��������
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
#if OS_MAX_QS == 1u             //������ֻ��һ�����п��ƿ�
	OSQFreeList = &OSQTbl[0];	/* Only ONE queue!                                */
	OSQFreeList->OSQPtr = (OS_Q *) 0;
#endif

#if OS_MAX_QS >= 2u             //�������ж�����п��ƿ�
	INT16U ix;
	INT16U ix_next;
	OS_Q *pq1;
	OS_Q *pq2;



	OS_MemClr((INT8U *) & OSQTbl[0], sizeof(OSQTbl));	/*���еĿ��ƿ����� Clear the queue table                          */
	for (ix = 0u; ix < (OS_MAX_QS - 1u); ix++) {	/*ÿ�����п��ƿ������ʼ�� Init. list of free QUEUE control blocks        */
		ix_next = ix + 1u;
		pq1 = &OSQTbl[ix];
		pq2 = &OSQTbl[ix_next];
		pq1->OSQPtr = pq2;                          //�γɵ�������
	}
	pq1 = &OSQTbl[ix];
	pq1->OSQPtr = (OS_Q *) 0;                       //���һ�����ƿ�ָ��0
	OSQFreeList = &OSQTbl[0];                       //OSQFreeListָ������ͷ
#endif
}
#endif				/* OS_Q_EN                                        */
