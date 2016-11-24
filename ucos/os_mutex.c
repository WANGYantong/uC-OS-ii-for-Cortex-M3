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
* If you plan on using  uC/OS-II  in a commercial product you need to contact Micri�m to properly license
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

#define  OS_MUTEX_KEEP_LOWER_8   ((INT16U)0x00FFu)      //ȡ����ֵ�ĵͰ�λ
#define  OS_MUTEX_KEEP_UPPER_8   ((INT16U)0xFF00u)      //ȡ����ֵ�ĸ߰�λ

#define  OS_MUTEX_AVAILABLE      ((INT16U)0x00FFu)      //������ʱ�����ź����ǿ��Ի�ȡ��

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
*              �޵ȴ��Ļ�ȡ�����ź���
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
*              ����Ϣ���С���Ϣ���䡢�ź�����ͬ�������ź������޵ȴ���ȡ���ܱ�ISR���ã���Ϊ�����ź���ֻ�ܱ�����ʹ��
*              ������жϵ����ˣ��ж�Ҳ�ܽ������ȼ��̳�ô?˭֪���жϻ�ʲôʱ���������ͷ��ź������������л���
********************************************************************************************************
*/

#if OS_MUTEX_ACCEPT_EN > 0u
BOOLEAN OSMutexAccept(OS_EVENT * pevent, INT8U * perr)
{
	INT8U pip;		/* Priority Inheritance Priority (PIP)          */
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register     */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*ȷ��ECB���� Validate 'pevent'                            */
		*perr = OS_ERR_PEVENT_NULL;
		return (OS_FALSE);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {	/*ȷ��ECB�������ǻ����ź������� Validate event block type                    */
		*perr = OS_ERR_EVENT_TYPE;
		return (OS_FALSE);
	}
	if (OSIntNesting > 0u) {	/*ȷ���������жϷ�������� Make sure it's not called from an ISR        */
		*perr = OS_ERR_PEND_ISR;
		return (OS_FALSE);
	}
	OS_ENTER_CRITICAL();	/* Get value (0 or 1) of Mutex                  */
	pip = (INT8U) (pevent->OSEventCnt >> 8u);	/*��ȡ�����ź�����PIP Get PIP from mutex                           */
	if ((pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8) == OS_MUTEX_AVAILABLE) {//����������ĵͰ�λΪ0xFF,��ζ�Ŵ˿̻����ź���û�б�ռ��
		pevent->OSEventCnt &= OS_MUTEX_KEEP_UPPER_8;	/*����ź����������Ͱ�λ      Mask off LSByte (Acquire Mutex)         */
		pevent->OSEventCnt |= OSTCBCur->OSTCBPrio;	/*���ݵ�ǰ����(������Ϊ�ź���ӵ����)�����ȼ�      Save current task priority in LSByte    */
		pevent->OSEventPtr = (void *) OSTCBCur;	/*�����ź�����ӵ����ָ��ָ��ǰ����      Link TCB of task owning Mutex           */
		if (OSTCBCur->OSTCBPrio <= pip) {	/* �����ǰ��������ȼ����и���PIP     PIP 'must' have a SMALLER prio ...      */
			OS_EXIT_CRITICAL();	/*      ... than current task!                  */
			*perr = OS_ERR_PIP_LOWER;       //���ش�����룬��ζ��PIP���õ�������
		} else {
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_NONE;            //PIP���õ�ûë��
		}
		return (OS_TRUE);                   //��ȡ���˻����ź���
	}
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
	return (OS_FALSE);                      //��ǰ������ռ�л����ź�������ȡʧ��
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                  CREATE A MUTUAL EXCLUSION SEMAPHORE
*
* Description: This function creates a mutual exclusion semaphore.
*              ����һ�������ź���
*
* Arguments  : prio          is the priority to use when accessing the mutual exclusion semaphore.  In
*                            other words, when the semaphore is acquired and a higher priority task
*                            attempts to obtain the semaphore then the priority of the task owning the
*                            semaphore is raised to this priority.  It is assumed that you will specify
*                            a priority that is LOWER in value than ANY of the tasks competing for the
*                            mutex.
*                            ���ȼ��̳е����ȼ���PIP
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
*                 ����ֵ�ĵͰ�λ�����ȡ�������ź�������������ȼ������û�������ȡ������ֵΪ0xFF
*              2) The MOST  significant 8 bits of '.OSEventCnt' are used to hold the priority number
*                 to use to reduce priority inversion.
*                 �߰�λ����PIP
*********************************************************************************************************
*/

OS_EVENT *OSMutexCreate(INT8U prio, INT8U * perr)
{
	OS_EVENT *pevent;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //������õ����ַ��������жϣ���ô��Ҫcpu_sr�������ж�״̬
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
	if (prio >= OS_LOWEST_PRIO) {	/*��֤PIP�Ϸ� Validate PIP                             */
		*perr = OS_ERR_PRIO_INVALID;
		return ((OS_EVENT *) 0);
	}
#endif
	if (OSIntNesting > 0u) {	/*ȷ���������жϷ�������� See if called from ISR ...               */
		*perr = OS_ERR_CREATE_ISR;	/* ... can't CREATE mutex from an ISR       */
		return ((OS_EVENT *) 0);
	}
	OS_ENTER_CRITICAL();
	if (OSTCBPrioTbl[prio] != (OS_TCB *) 0) {	/*���PIP�Ƿ��Ѿ���ռ�� Mutex priority must not already exist    */
		OS_EXIT_CRITICAL();	/* Task already exist at priority ...       */
		*perr = OS_ERR_PRIO_EXIST;	/*��ռ�ã����ش������ ... inheritance priority                 */
		return ((OS_EVENT *) 0);
	}
    //PIPû��ռ�ã�����ʹ��
	OSTCBPrioTbl[prio] = OS_TCB_RESERVED;	/*����PIP��Ӧ�����ȼ����� Reserve the table entry                  */
	pevent = OSEventFreeList;	/*�ӿ����¼�������ȡ��һ���¼����ƿ� Get next free event control block        */
	if (pevent == (OS_EVENT *) 0) {	/*���ȡ����ECB������  See if an ECB was available              */
		OSTCBPrioTbl[prio] = (OS_TCB *) 0;	/*�ͷŸո�ռ�õ�PIP��Ӧ�����ȼ������е�λ�� No, Release the table entry              */
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_PEVENT_NULL;	/*���ش������ No more event control blocks             */
		return (pevent);
	}
    //ȡ����ECB����
	OSEventFreeList = (OS_EVENT *) OSEventFreeList->OSEventPtr;	/*���������¼����еĶ���ָ�� Adjust the free list        */
	OS_EXIT_CRITICAL();
	pevent->OSEventType = OS_EVENT_TYPE_MUTEX;  //���¼���������Ϊ�����ź�������
	pevent->OSEventCnt = (INT16U) ((INT16U) prio << 8u) | OS_MUTEX_AVAILABLE;	/*����ֵ�߰�λ��ΪPIP���Ͱ�λ��Ϊ0xFF Resource is avail.  */
	pevent->OSEventPtr = (void *) 0;	/*��ʱû������ռ�û����ź��� No task owning the mutex    */
#if OS_EVENT_NAME_EN > 0u
	pevent->OSEventName = (INT8U *) (void *) "?"; //��ʼ��������
#endif
	OS_EventWaitListInit(pevent);                 //����ĵȴ����ʼ��
	*perr = OS_ERR_NONE;
	return (pevent);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                          DELETE A MUTEX
*
* Description: This function deletes a mutual exclusion semaphore and readies all tasks pending on the it.
*              ɾ�������ź���
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired mutex.
*
*              opt           determines delete options as follows:
*                            ����ɾ������ѡ��
*                            opt == OS_DEL_NO_PEND   Delete mutex ONLY if no task pending
*                                                    ֻ����û������ȴ�ʱ��ɾ���ź���
*                            opt == OS_DEL_ALWAYS    Deletes the mutex even if tasks are waiting.
*                                                    In this case, all the tasks pending will be readied.
*                                                    ������û������ȴ�����ɾ���ź������������еĵȴ�������Ϊ����
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
	OS_CPU_SR cpu_sr = 0u;      //ʹ�õ����ַ�ʽ�����жϣ���Ҫcpu_sr�����ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*ȷ��ECB���� Validate 'pevent'                        */
		*perr = OS_ERR_PEVENT_NULL;
		return (pevent);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {	/*ȷ��ECB�������ǻ����ź��� Validate event block type                */
		*perr = OS_ERR_EVENT_TYPE;
		return (pevent);
	}
	if (OSIntNesting > 0u) {	/*ȷ���������жϷ�������� See if called from ISR ...               */
		*perr = OS_ERR_DEL_ISR;	/* ... can't DELETE from an ISR             */
		return (pevent);
	}
	OS_ENTER_CRITICAL();
	if (pevent->OSEventGrp != 0u) {	/*�Ƿ��������ڵȴ� See if any tasks waiting on mutex        */
		tasks_waiting = OS_TRUE;	/* Yes                                      */
	} else {
		tasks_waiting = OS_FALSE;	/* No                                       */
	}
	switch (opt) {
	case OS_DEL_NO_PEND:	/*���������ֻ����û������ȴ��������²ſ���ɾ�������ź��� DELETE MUTEX ONLY IF NO TASK WAITING --- */
		if (tasks_waiting == OS_FALSE) {    //��ʱû������ȴ�
#if OS_EVENT_NAME_EN > 0u
			pevent->OSEventName = (INT8U *) (void *) "?";   //��ʼ���¼���
#endif
			pip = (INT8U) (pevent->OSEventCnt >> 8u);       //��ȡPIP
			OSTCBPrioTbl[pip] = (OS_TCB *) 0;	/*����ʹ�ö��ͷ�PIP Free up the PIP                          */
			pevent->OSEventType = OS_EVENT_TYPE_UNUSED;     //ECB���͸�Ϊδʹ������
			pevent->OSEventPtr = OSEventFreeList;	/*��ECB�黹�����¼����� Return Event Control Block to free list  */
			pevent->OSEventCnt = 0u;                //����ֵ����
			OSEventFreeList = pevent;               //���������¼����еĶ���ָ��
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_NONE;
			pevent_return = (OS_EVENT *) 0;	/* Mutex has been deleted                   */
		} else {                            //��ʱ������ȴ�������ɾ��
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_TASK_WAITING;    //���ش������
			pevent_return = pevent;
		}
		break;

	case OS_DEL_ALWAYS:	/*��������ǲ�����û������ȴ���ֱ��ɾ�������ź��� ALWAYS DELETE THE MUTEX ---------------- */
		pip = (INT8U) (pevent->OSEventCnt >> 8u);	/*��ȡPIP Get PIP of mutex          */
		prio = (INT8U) (pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8);	/*��ȡ��ǰ�����ź���ռ���ߵ�ԭ�������ȼ� Get owner's original prio */
		ptcb = (OS_TCB *) pevent->OSEventPtr;       //��ȡ�����ź���ӵ���ߵ�TCB
		if (ptcb != (OS_TCB *) 0) {	/*������ӵ�����Ǵ��ڵģ��������ź�����һ������ռ�� See if any task owns the mutex           */
			if (ptcb->OSTCBPrio == pip) {	/*���ӵ���ߵ����ȼ��Ƿ�̳���PIP See if original prio was changed         */
				OSMutex_RdyAtPrio(ptcb, prio);	/*����ǣ������ӵ���ߵ����ȼ���ԭΪ��ԭ�������ȼ� Yes, Restore the task's original prio    */
			}
		}
		while (pevent->OSEventGrp != 0u) {	/*�����еȴ������ź�����������Ϊ���� Ready ALL tasks waiting for mutex        */
			(void) OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_MUTEX, OS_STAT_PEND_OK);
		}
#if OS_EVENT_NAME_EN > 0u
		pevent->OSEventName = (INT8U *) (void *) "?";   //�����ź��������ֻ�ԭ
#endif
		pip = (INT8U) (pevent->OSEventCnt >> 8u);       //��ȡ����ʱ��PIP
		OSTCBPrioTbl[pip] = (OS_TCB *) 0;	/*����ʹ�ö��ͷ�PIP Free up the PIP                          */
		pevent->OSEventType = OS_EVENT_TYPE_UNUSED;     //��ECB����������Ϊδʹ��
		pevent->OSEventPtr = OSEventFreeList;	/*��ECB�黹�������¼����� Return Event Control Block to free list  */
		pevent->OSEventCnt = 0u;                //����ֵ����
		OSEventFreeList = pevent;	/*���������¼����еĶ���ָ�� Get next free event control block        */
		OS_EXIT_CRITICAL();
		if (tasks_waiting == OS_TRUE) {	/*���֮ǰ������ȴ� Reschedule only if task(s) were waiting  */
			OS_Sched();	/*����������� Find highest priority task ready to run  */
		}
		*perr = OS_ERR_NONE;
		pevent_return = (OS_EVENT *) 0;	/* Mutex has been deleted                   */
		break;

	default:            //�����������
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
*              �����ȡ�����ź���
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired
*                            mutex.
*
*              timeout       is an optional timeout period (in clock ticks).  If non-zero, your task will
*                            wait for the resource up to the amount of time specified by this argument.
*                            If you specify 0, however, your task will wait forever at the specified
*                            mutex or, until the resource becomes available.
*                            ��Ϊ0����ʾ�����ȴ�
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
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif


#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*ȷ��ECB���� Validate 'pevent'                        */
		*perr = OS_ERR_PEVENT_NULL;
		return;
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {	/*ȷ��ECB�������ǻ����ź��� Validate event block type                */
		*perr = OS_ERR_EVENT_TYPE;
		return;
	}
	if (OSIntNesting > 0u) {	/*ȷ���������жϷ������� See if called from ISR ...               */
		*perr = OS_ERR_PEND_ISR;	/* ... can't PEND from an ISR               */
		return;
	}
	if (OSLockNesting > 0u) {	/*ȷ������û������ See if called with scheduler locked ...  */
		*perr = OS_ERR_PEND_LOCKED;	/* ... can't PEND when locked               */
		return;
	}
/*$PAGE*/
	OS_ENTER_CRITICAL();
	pip = (INT8U) (pevent->OSEventCnt >> 8u);	/*��ȡ�����ź�����PIP Get PIP from mutex                       */
	/* Is Mutex available? ����ź������ã��������ֵ�ĵͰ�λӦ��Ϊ0xFF                      */
	if ((INT8U) (pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8) == OS_MUTEX_AVAILABLE) {   //�����ź���û�б�ռ��
		pevent->OSEventCnt &= OS_MUTEX_KEEP_UPPER_8;	/*���������ĵͰ�λ���㣬�������ݸ�����ǰ�����ȼ� Yes, Acquire the resource                */
		pevent->OSEventCnt |= OSTCBCur->OSTCBPrio;	/*���ݸ���������ȼ��������Ժ�ָ�      Save priority of owning task        */
		pevent->OSEventPtr = (void *) OSTCBCur;	/*�����ź�����ӵ�����ǵ�ǰ����      Point to owning task's OS_TCB       */
		if (OSTCBCur->OSTCBPrio <= pip) {	/*�����ǰ��������ȼ�Ҫ������PIP      PIP 'must' have a SMALLER prio ...  */
			OS_EXIT_CRITICAL();	/*      ... than current task!              */
			*perr = OS_ERR_PIP_LOWER;   //˵��PIP���õ������⣬���ش������
		} else {                        //PIP���õ�ûë��
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_NONE;
		}
		return;
	}
    //�������Ļ����ź����Ѿ���ռ��
	mprio = (INT8U) (pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8);	/*��ȡ�ź���ӵ���ߵ�ԭ�������ȼ� No, Get priority of mutex owner   */
	ptcb = (OS_TCB *) (pevent->OSEventPtr);	/*�ź���ӵ���ߵ�TCB     Point to TCB of mutex owner   */
	if (ptcb->OSTCBPrio > pip) {	/*����ź���ӵ���ߵ����ȼ�Ҫ����PIP     Need to promote prio of owner? */
		if (mprio > OSTCBCur->OSTCBPrio) {//�����ź���ӵ���ߵ����ȼ��ȵ�ǰ����(�ź�����������)Ҫ��
			y = ptcb->OSTCBY;
			if ((OSRdyTbl[y] & ptcb->OSTCBBitX) != 0u) {	/*����ź���ӵ�����Ƿ����     See if mutex owner is ready   */
				OSRdyTbl[y] &= (OS_PRIO) ~ ptcb->OSTCBBitX;	/*���������ʹ�ź���ӵ���������������ΪҪ�������ȼ��̳У���ΪPIP  Yes, Remove owner from Rdy ... */
				if (OSRdyTbl[y] == 0u) {	/*          ... list at current prio */
					OSRdyGrp &= (OS_PRIO) ~ ptcb->OSTCBBitY;
				}
				rdy = OS_TRUE;                              //����Ϊ����״̬��־
			} else {                                //����ź���ӵ���߲��Ǿ���״̬
				pevent2 = ptcb->OSTCBEventPtr;      //��ȡʹ���ź���ӵ���ߵȴ����¼�
				if (pevent2 != (OS_EVENT *) 0) {	/*������¼����ڣ����ź���ӵ���ߴӵȴ��б���ɾ������������µ����ȼ����� Remove from event wait list       */
					y = ptcb->OSTCBY;
					pevent2->OSEventTbl[y] &= (OS_PRIO) ~ ptcb->OSTCBBitX;
					if (pevent2->OSEventTbl[y] == 0u) {
						pevent2->OSEventGrp &= (OS_PRIO) ~ ptcb->OSTCBBitY;
					}
				}
				rdy = OS_FALSE;	/* ����Ϊδ����״̬��־ No                                       */
			}
			ptcb->OSTCBPrio = pip;	/*�ź�����ӵ���߽������ȼ��ļ̳У���ΪPIP Change owner task prio to PIP            */
#if OS_LOWEST_PRIO <= 63u
			ptcb->OSTCBY = (INT8U) (ptcb->OSTCBPrio >> 3u);
			ptcb->OSTCBX = (INT8U) (ptcb->OSTCBPrio & 0x07u);
#else
			ptcb->OSTCBY = (INT8U) ((INT8U) (ptcb->OSTCBPrio >> 4u) & 0xFFu);
			ptcb->OSTCBX = (INT8U) (ptcb->OSTCBPrio & 0x0Fu);
#endif
			ptcb->OSTCBBitY = (OS_PRIO) (1uL << ptcb->OSTCBY);
			ptcb->OSTCBBitX = (OS_PRIO) (1uL << ptcb->OSTCBX);

			if (rdy == OS_TRUE) {	/*����ź���ӵ���������ȼ��̳�֮ǰ�Ǿ����� If task was ready at owner's priority ... */
				OSRdyGrp |= ptcb->OSTCBBitY;	/*ʹ֮���µ����ȼ����� ... make it ready at new priority.       */
				OSRdyTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
			} else {                //����ź���ӵ���������ȼ��̳�֮ǰ�ǵȴ��ģ�������ʹ�����µ����ȼ��ȴ�
				pevent2 = ptcb->OSTCBEventPtr;
				if (pevent2 != (OS_EVENT *) 0) {	/* Add to event wait list                   */
					pevent2->OSEventGrp |= ptcb->OSTCBBitY;
					pevent2->OSEventTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
				}
			}
			OSTCBPrioTbl[pip] = ptcb;   //���ź���ӵ���ߵ�TCB��PIP���ȼ�����ݱ��������ȼ��б���
		}
	}
	OSTCBCur->OSTCBStat |= OS_STAT_MUTEX;	/*���ź��������ߵ�״̬��Ϊ�ȴ������ź��� Mutex not available, pend current task        */
	OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;  //���ź�����������Ϊ����״̬
	OSTCBCur->OSTCBDly = timeout;	/*����ȴ����¼� Store timeout in current task's TCB           */
	OS_EventTaskWait(pevent);	/*���ź��������߹��� Suspend task until event or timeout occurs    */
	OS_EXIT_CRITICAL();
	OS_Sched();		/*��ǰ���񱻹������½���������� Find next highest priority task ready         */
    //�ź��������߹������������ԭ��:��ʱ?��ֹ?���ǻ�ȡ���˻����ź���?
	OS_ENTER_CRITICAL();
	switch (OSTCBCur->OSTCBStatPend) {	/* See if we timed-out or aborted                */
	case OS_STAT_PEND_OK:           //��Ϊ��ȡ���˻����ź���
		*perr = OS_ERR_NONE;
		break;

	case OS_STAT_PEND_ABORT:
		*perr = OS_ERR_PEND_ABORT;	/*��Ϊ��������ֹ�ȴ��ĺ��� Indicate that we aborted getting mutex        */
		break;

	case OS_STAT_PEND_TO:           //��Ϊ�ȴ���ʱ
	default:
		OS_EventTaskRemove(OSTCBCur, pevent);   //���ź��������ߴӻ����ź����ĵȴ�������ɾ��
		*perr = OS_ERR_TIMEOUT;	/* Indicate that we didn't get mutex within TO   */
		break;
	}
	OSTCBCur->OSTCBStat = OS_STAT_RDY;	/*���ź�����������Ϊ����̬ Set   task  status to ready                   */
	OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;	/*��������־ Clear pend  status                            */
	OSTCBCur->OSTCBEventPtr = (OS_EVENT *) 0;	/*���ȴ����¼�ָ����Ϊ�� Clear event pointers                          */
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
*              �ͷŻ����ź���
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
	OS_CPU_SR cpu_sr = 0u;      //���÷������������жϣ���Ҫcpu_sr�������ж�״̬
#endif



	if (OSIntNesting > 0u) {	/*ȷ���������жϷ�������� See if called from ISR ...                    */
		return (OS_ERR_POST_ISR);	/* ... can't POST mutex from an ISR              */
	}
#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*ȷ��ECB���� Validate 'pevent'                             */
		return (OS_ERR_PEVENT_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {	/*ȷ��ECB�������ǻ����ź������� Validate event block type                     */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	pip = (INT8U) (pevent->OSEventCnt >> 8u);	/*��ȡ�����ź���PIP Get priority inheritance priority of mutex    */
	prio = (INT8U) (pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8);	/*��ȡ�����ź���ӵ����ԭ�������ȼ� Get owner's original priority      */
	if (OSTCBCur != (OS_TCB *) pevent->OSEventPtr) {	/*��ǰ�����Ƿ��ǻ����ź�����ӵ���� See if posting task owns the MUTEX            */
		OS_EXIT_CRITICAL();
		return (OS_ERR_NOT_MUTEX_OWNER);                //������Ƿ��ش�����룬ֻ�л����ź�����ӵ���߲ſ����ͷŻ����ź���
	}
	if (OSTCBCur->OSTCBPrio == pip) {	/*�����ź���ӵ���ߵ����ȼ��Ƿ���PIP Did we have to raise current task's priority? */
		OSMutex_RdyAtPrio(OSTCBCur, prio);	/*����ǣ���ζ�Ž��й����ȼ��̳У���ӵ���ߵ����ȼ���ԭΪ��ԭ�������ȼ� Restore the task's original priority          */
	}
	OSTCBPrioTbl[pip] = OS_TCB_RESERVED;	/*ռ��PIP���ȼ��б� Reserve table entry                           */
	if (pevent->OSEventGrp != 0u) {	/*��ǰ�������ڵȴ������ź���ô Any task waiting for the mutex?               */
		/*����У���HPT���� Yes, Make HPT waiting for mutex ready         */
		prio = OS_EventTaskRdy(pevent, (void *) 0, OS_STAT_MUTEX, OS_STAT_PEND_OK); //��HPT����������ȡ�����ȼ�
		pevent->OSEventCnt &= OS_MUTEX_KEEP_UPPER_8;	/*�����ź����������Ͱ�λ��0      Save priority of mutex's new owner       */
		pevent->OSEventCnt |= prio;                     //�����µ��ź���ӵ���ߵ�ԭ�������ȼ�
		pevent->OSEventPtr = OSTCBPrioTbl[prio];	/*�����ź�����ӵ����ָ��ָ���µ�ӵ����      Link to new mutex owner's OS_TCB         */
		if (prio <= pip) {	/*�������µ�ӵ���ߵ����ȼ���Ҫ����PIP      PIP 'must' have a SMALLER prio ...       */
			OS_EXIT_CRITICAL();	/*      ... than current task!                   */
			OS_Sched();	/*�������      Find highest priority task ready to run  */
			return (OS_ERR_PIP_LOWER);  //���ش�����룬PIP���õ�������
		} else {            //PIP���ȼ������µ�ӵ���ߣ�ûë��
			OS_EXIT_CRITICAL();
			OS_Sched();	/* �������     Find highest priority task ready to run  */
			return (OS_ERR_NONE);
		}
	}
    //���û�������ڵȴ������ź���
	pevent->OSEventCnt |= OS_MUTEX_AVAILABLE;	/*�ź�������ֵ�Ͱ�λ��Ϊ0xFF No,  Mutex is now available                   */
	pevent->OSEventPtr = (void *) 0;           //ӵ����ָ����Ϊ��
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                     QUERY A MUTUAL EXCLUSION SEMAPHORE
*
* Description: This function obtains information about a mutex
*              ��ȡ��ǰ�����ź�����״̬
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
	OS_CPU_SR cpu_sr = 0u;      //���÷�ʽ�������жϣ�������Ҫcpu_sr�����ж�״̬
#endif



	if (OSIntNesting > 0u) {	/*ȷ�������жϷ�������� See if called from ISR ...               */
		return (OS_ERR_QUERY_ISR);	/* ... can't QUERY mutex from an ISR        */
	}
#if OS_ARG_CHK_EN > 0u
	if (pevent == (OS_EVENT *) 0) {	/*ȷ��ECB���� Validate 'pevent'                        */
		return (OS_ERR_PEVENT_NULL);
	}
	if (p_mutex_data == (OS_MUTEX_DATA *) 0) {	/*ȷ�������ѯ��������ݽṹ���� Validate 'p_mutex_data'                  */
		return (OS_ERR_PDATA_NULL);
	}
#endif
	if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {	/*ȷ���ź����������ǻ����ź��� Validate event block type                */
		return (OS_ERR_EVENT_TYPE);
	}
	OS_ENTER_CRITICAL();
	p_mutex_data->OSMutexPIP = (INT8U) (pevent->OSEventCnt >> 8u);  //����PIP
	p_mutex_data->OSOwnerPrio = (INT8U) (pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8);//�����ź���ӵ����ԭ�������ȼ�
	if (p_mutex_data->OSOwnerPrio == 0xFFu) {   //�����һ�������Ľ����0xFF
		p_mutex_data->OSValue = OS_TRUE;        //��ζ��û������ռ�л����ź��������ź����ǿ��õ�
	} else {
		p_mutex_data->OSValue = OS_FALSE;       //��֮�����ǲ����õ�
	}
	p_mutex_data->OSEventGrp = pevent->OSEventGrp;	/*�����ź����еĵȴ��� Copy wait list                           */
	psrc = &pevent->OSEventTbl[0];                  //�ź����еĵȴ���ĵ�ַ
	pdest = &p_mutex_data->OSEventTbl[0];           //��������ݽṹ�еȴ���ĵ�ַ
	for (i = 0u; i < OS_EVENT_TBL_SIZE; i++) {      //���ź����еĵȴ�����������ο�������������ݽṹ��
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
*              ��ptcb�����ȼ��ָ�Ϊprio
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

    //��ʱptcb����PIP�����ȼ������ھ������еģ���ptcb�Ӿ�������ɾ��
	y = ptcb->OSTCBY;	/* Remove owner from ready list at 'pip'    */
	OSRdyTbl[y] &= (OS_PRIO) ~ ptcb->OSTCBBitX;
	if (OSRdyTbl[y] == 0u) {
		OSRdyGrp &= (OS_PRIO) ~ ptcb->OSTCBBitY;
	}

	ptcb->OSTCBPrio = prio; //ptcb�ָ�ԭ�������ȼ�
	OSPrioCur = prio;	/*��ǰ��������ȼ�Ҳһ���ָ� The current task is now at this priority */

    //����ptcb�ڲ����ٲ����������ȼ��ı���
#if OS_LOWEST_PRIO <= 63u
	ptcb->OSTCBY = (INT8U) ((INT8U) (prio >> 3u) & 0x07u);
	ptcb->OSTCBX = (INT8U) (prio & 0x07u);
#else
	ptcb->OSTCBY = (INT8U) ((INT8U) (prio >> 4u) & 0x0Fu);
	ptcb->OSTCBX = (INT8U) (prio & 0x0Fu);
#endif
	ptcb->OSTCBBitY = (OS_PRIO) (1uL << ptcb->OSTCBY);
	ptcb->OSTCBBitX = (OS_PRIO) (1uL << ptcb->OSTCBX);

    //��ptcb���¼����������ʱptcb���Իָ�������ȼ������ھ�������
	OSRdyGrp |= ptcb->OSTCBBitY;	/* Make task ready at original priority     */
	OSRdyTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
    //��ptcb���뵽��Ӧ�����ȼ�����
	OSTCBPrioTbl[prio] = ptcb;

}


#endif				/* OS_MUTEX_EN                              */
