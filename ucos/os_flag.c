/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*                                         EVENT FLAG  MANAGEMENT
*
*                              (c) Copyright 1992-2009, Micrium, Weston, FL
*                                           All Rights Reserved
*
* File    : OS_FLAG.C
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

#if (OS_FLAG_EN > 0u) && (OS_MAX_FLAGS > 0u)
/*
*********************************************************************************************************
*                                            LOCAL PROTOTYPES
*********************************************************************************************************
*/

static void OS_FlagBlock(OS_FLAG_GRP * pgrp, OS_FLAG_NODE * pnode, OS_FLAGS flags, INT8U wait_type, INT32U timeout);
static BOOLEAN OS_FlagTaskRdy(OS_FLAG_NODE * pnode, OS_FLAGS flags_rdy);

/*$PAGE*/
/*
*********************************************************************************************************
*                              CHECK THE STATUS OF FLAGS IN AN EVENT FLAG GROUP
*
* Description: This function is called to check the status of a combination of bits to be set or cleared
*              in an event flag group.  Your application can check for ANY bit to be set/cleared or ALL
*              bits to be set/cleared.
*
*              This call does not block if the desired flags are not present.
*              �޵ȴ��Ļ�ȡ�¼���־���е��¼���־
*
* Arguments  : pgrp          is a pointer to the desired event flag group.
*                            ָ���¼���־���ָ��
*
*              flags         Is a bit pattern indicating which bit(s) (i.e. flags) you wish to check.
*                            The bits you want are specified by setting the corresponding bits in
*                            'flags'.  e.g. if your application wants to wait for bits 0 and 1 then
*                            'flags' would contain 0x03.
*                            ��Ҫ�����¼���־λ�����Ϊ1�������Ӧλ��Ϊ0����ԡ�
*
*              wait_type     specifies whether you want ALL bits to be set/cleared or ANY of the bits
*                            to be set/cleared.�ȴ��¼���־λ�ķ�ʽ
*                            You can specify the following argument:
*
*                            OS_FLAG_WAIT_CLR_ALL   You will check ALL bits in 'flags' to be clear (0)
*                            OS_FLAG_WAIT_CLR_ANY   You will check ANY bit  in 'flags' to be clear (0)
*                            OS_FLAG_WAIT_SET_ALL   You will check ALL bits in 'flags' to be set   (1)
*                            OS_FLAG_WAIT_SET_ANY   You will check ANY bit  in 'flags' to be set   (1)
*
*                            NOTE: Add OS_FLAG_CONSUME if you want the event flag to be 'consumed' by
*                                  the call.  Example, to wait for any flag in a group AND then clear
*                                  the flags that are present, set 'wait_type' to:
*
*                                  OS_FLAG_WAIT_SET_ANY + OS_FLAG_CONSUME
*                                  OS_FLAG_CONSUME��ʾ�����ڻ������Ҫ���¼���־λ��Ҫ����ָ�
*
*              perr          is a pointer to an error code and can be:
*                            OS_ERR_NONE               No error
*                            OS_ERR_EVENT_TYPE         You are not pointing to an event flag group
*                            OS_ERR_FLAG_WAIT_TYPE     You didn't specify a proper 'wait_type' argument.
*                            OS_ERR_FLAG_INVALID_PGRP  You passed a NULL pointer instead of the event flag
*                                                      group handle.
*                            OS_ERR_FLAG_NOT_RDY       The desired flags you are waiting for are not
*                                                      available.
*
* Returns    : The flags in the event flag group that made the task ready or, 0 if a timeout or an error
*              occurred.
*
* Called from: Task or ISR
*
* Note(s)    : 1) IMPORTANT, the behavior of this function has changed from PREVIOUS versions.  The
*                 function NOW returns the flags that were ready INSTEAD of the current state of the
*                 event flags.
*********************************************************************************************************
*/

#if OS_FLAG_ACCEPT_EN > 0u
OS_FLAGS OSFlagAccept(OS_FLAG_GRP * pgrp, OS_FLAGS flags, INT8U wait_type, INT8U * perr)
{
	OS_FLAGS flags_rdy;
	INT8U result;
	BOOLEAN consume;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pgrp == (OS_FLAG_GRP *) 0) {	/*ȷ���¼���־��ָ����� Validate 'pgrp'                          */
		*perr = OS_ERR_FLAG_INVALID_PGRP;
		return ((OS_FLAGS) 0);
	}
#endif
	if (pgrp->OSFlagType != OS_EVENT_TYPE_FLAG) {	/*ȷ��������ȷ Validate event block type                */
		*perr = OS_ERR_EVENT_TYPE;
		return ((OS_FLAGS) 0);
	}
	result = (INT8U) (wait_type & OS_FLAG_CONSUME); //��ȡ�������͵�consumeλ
	if (result != (INT8U) 0) {	/*�Ƿ�ʹ����consume See if we need to consume the flags      */
		wait_type &= ~OS_FLAG_CONSUME;      //����ǣ���consumeλ���
		consume = OS_TRUE;                  //��־ʹ����consume
	} else {
		consume = OS_FALSE;
	}
/*$PAGE*/
	*perr = OS_ERR_NONE;	/* Assume NO error until proven otherwise.  */
	OS_ENTER_CRITICAL();
	switch (wait_type) {
	case OS_FLAG_WAIT_SET_ALL:	/*��Ҫ���е�����λ��1 See if all required flags are set        */
		flags_rdy = (OS_FLAGS) (pgrp->OSFlagFlags & flags);	/*��ȡ����Ҫ��λ Extract only the bits we want   */
		if (flags_rdy == flags) {	/*�Ƿ����Ҫ��ȫ����1 Must match ALL the bits that we want     */
			if (consume == OS_TRUE) {	/*���ʹ����consume See if we need to consume the flags      */
				pgrp->OSFlagFlags &= (OS_FLAGS) ~ flags_rdy;	/*�ָ�֮ǰ��״̬ Clear ONLY the flags we wanted  */
			}
		} else {                            //������Ҫ��û�л�ȡ���¼���־
			*perr = OS_ERR_FLAG_NOT_RDY;
		}
		OS_EXIT_CRITICAL();
		break;

	case OS_FLAG_WAIT_SET_ANY:  //��Ҫ����һ������λΪ1
		flags_rdy = (OS_FLAGS) (pgrp->OSFlagFlags & flags);	/*��ȡ����Ҫ��λ Extract only the bits we want   */
		if (flags_rdy != (OS_FLAGS) 0) {	/*�Ƿ����Ҫ�󣬲�ȫΪ0������һλ��1 See if any flag set                      */
			if (consume == OS_TRUE) {	/*���ʹ����consume See if we need to consume the flags      */
				pgrp->OSFlagFlags &= (OS_FLAGS) ~ flags_rdy;	/*�ָ�֮ǰ��״̬ Clear ONLY the flags we got     */
			}
		} else {                            //������Ҫ��û�л�ȡ���¼���־
			*perr = OS_ERR_FLAG_NOT_RDY;
		}
		OS_EXIT_CRITICAL();
		break;

#if OS_FLAG_WAIT_CLR_EN > 0u
	case OS_FLAG_WAIT_CLR_ALL:	/*��Ҫ���е�����λΪ0 See if all required flags are cleared    */
		flags_rdy = (OS_FLAGS) ~ pgrp->OSFlagFlags & flags;	/*��ȡ����Ҫ��λ������Ҫȡ�����1 Extract only the bits we want     */
		if (flags_rdy == flags) {	/*�Ƿ����Ҫ�����е�ȡ����Ϊ1 Must match ALL the bits that we want     */
			if (consume == OS_TRUE) {	/*���ʹ����consume See if we need to consume the flags      */
				pgrp->OSFlagFlags |= flags_rdy;	/*�ָ�֮ǰ��״̬ Set ONLY the flags that we wanted        */
			}
		} else {                            //������Ҫ��û�л�ȡ���¼���־
			*perr = OS_ERR_FLAG_NOT_RDY;
		}
		OS_EXIT_CRITICAL();
		break;

	case OS_FLAG_WAIT_CLR_ANY:  //��Ҫ����һ������λΪ0
		flags_rdy = (OS_FLAGS) ~ pgrp->OSFlagFlags & flags;	/*��ȡ����Ҫ��λ������ȡ�����1 Extract only the bits we want      */
		if (flags_rdy != (OS_FLAGS) 0) {	/*�Ƿ����Ҫ��ȡ��������һλΪ1 See if any flag cleared                  */
			if (consume == OS_TRUE) {	/*���ʹ����consume See if we need to consume the flags      */
				pgrp->OSFlagFlags |= flags_rdy;	/*�ָ�֮ǰ��״̬ Set ONLY the flags that we got           */
			}
		} else {                            //������Ҫ��û�л�ȡ���¼���־
			*perr = OS_ERR_FLAG_NOT_RDY;
		}
		OS_EXIT_CRITICAL();
		break;
#endif

	default:                        //�������Ϸ�
		OS_EXIT_CRITICAL();
		flags_rdy = (OS_FLAGS) 0;
		*perr = OS_ERR_FLAG_WAIT_TYPE;
		break;
	}
	return (flags_rdy);
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                           CREATE AN EVENT FLAG
*
* Description: This function is called to create an event flag group.
*              �����¼���־��
*
* Arguments  : flags         Contains the initial value to store in the event flag group.  �¼���־��ʼֵ
*
*              perr          is a pointer to an error code which will be returned to your application:
*                               OS_ERR_NONE               if the call was successful.
*                               OS_ERR_CREATE_ISR         if you attempted to create an Event Flag from an
*                                                         ISR.
*                               OS_ERR_FLAG_GRP_DEPLETED  if there are no more event flag groups
*
* Returns    : A pointer to an event flag group or a NULL pointer if no more groups are available.
*
* Called from: Task ONLY
*********************************************************************************************************
*/

OS_FLAG_GRP *OSFlagCreate(OS_FLAGS flags, INT8U * perr)
{
	OS_FLAG_GRP *pgrp;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register        */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
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

	if (OSIntNesting > 0u) {	/*�������жϷ��������ṩ See if called from ISR ...                      */
		*perr = OS_ERR_CREATE_ISR;	/* ... can't CREATE from an ISR                    */
		return ((OS_FLAG_GRP *) 0);
	}
	OS_ENTER_CRITICAL();
	pgrp = OSFlagFreeList;	/*�ӿ����¼���־��������ȡ��һ�� Get next free event flag                        */
	if (pgrp != (OS_FLAG_GRP *) 0) {	/*ȡ�µ��ǿ��õ� See if we have event flag groups available      */
		/* Adjust free list                                */
		OSFlagFreeList = (OS_FLAG_GRP *) OSFlagFreeList->OSFlagWaitList;  //���������¼���־���������ָ��
		pgrp->OSFlagType = OS_EVENT_TYPE_FLAG;	/*�����¼���־������ Set to event flag group type                    */
		pgrp->OSFlagFlags = flags;	/*���ó�ʼֵ Set to desired initial value                    */
		pgrp->OSFlagWaitList = (void *) 0;	/*��յȴ������б� Clear list of tasks waiting on flags            */
#if OS_FLAG_NAME_EN > 0u
		pgrp->OSFlagName = (INT8U *) (void *) "?";  //����
#endif
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_NONE;
	} else {
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_FLAG_GRP_DEPLETED;
	}
	return (pgrp);		/* Return pointer to event flag group              */
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                     DELETE AN EVENT FLAG GROUP
*
* Description: This function deletes an event flag group and readies all tasks pending on the event flag
*              group.
*              ɾ���¼���־�飬����������������Ϊ����̬
*
* Arguments  : pgrp          is a pointer to the desired event flag group.
*                            ָ���¼���־���ָ��
*
*              opt           determines delete options as follows:ָ��ɾ������
*                            opt == OS_DEL_NO_PEND   Deletes the event flag group ONLY if no task pending
*                                                    ��û������ȴ���ʱ��ɾ���¼���־��
*                            opt == OS_DEL_ALWAYS    Deletes the event flag group even if tasks are
*                                                    waiting.  In this case, all the tasks pending will be
*                                                    readied.
*                                                    ������û�������ڵȴ���ɾ���¼���־�飬�����ȴ����������
*
*              perr          is a pointer to an error code that can contain one of the following values:
*                            OS_ERR_NONE               The call was successful and the event flag group was
*                                                      deleted
*                            OS_ERR_DEL_ISR            If you attempted to delete the event flag group from
*                                                      an ISR
*                            OS_ERR_FLAG_INVALID_PGRP  If 'pgrp' is a NULL pointer.
*                            OS_ERR_EVENT_TYPE         If you didn't pass a pointer to an event flag group
*                            OS_ERR_INVALID_OPT        An invalid option was specified
*                            OS_ERR_TASK_WAITING       One or more tasks were waiting on the event flag
*                                                      group.
*
* Returns    : pgrp          upon error
*              (OS_EVENT *)0 if the event flag group was successfully deleted.
*
* Note(s)    : 1) This function must be used with care.  Tasks that would normally expect the presence of
*                 the event flag group MUST check the return code of OSFlagAccept() and OSFlagPend().
*              2) This call can potentially disable interrupts for a long time.  The interrupt disable
*                 time is directly proportional to the number of tasks waiting on the event flag group.
*********************************************************************************************************
*/

#if OS_FLAG_DEL_EN > 0u
OS_FLAG_GRP *OSFlagDel(OS_FLAG_GRP * pgrp, INT8U opt, INT8U * perr)
{
	BOOLEAN tasks_waiting;
	OS_FLAG_NODE *pnode;
	OS_FLAG_GRP *pgrp_return;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pgrp == (OS_FLAG_GRP *) 0) {	/*ȷ��������¼���־����� Validate 'pgrp'                          */
		*perr = OS_ERR_FLAG_INVALID_PGRP;
		return (pgrp);
	}
#endif
	if (OSIntNesting > 0u) {	/*ȷ�������жϷ������е��� See if called from ISR ...               */
		*perr = OS_ERR_DEL_ISR;	/* ... can't DELETE from an ISR             */
		return (pgrp);
	}
	if (pgrp->OSFlagType != OS_EVENT_TYPE_FLAG) {	/*ȷ���������¼���־���� Validate event group type                */
		*perr = OS_ERR_EVENT_TYPE;
		return (pgrp);
	}
	OS_ENTER_CRITICAL();
	if (pgrp->OSFlagWaitList != (void *) 0) {	/*�ȴ���������û������ See if any tasks waiting on event flags  */
		tasks_waiting = OS_TRUE;	/* Yes                                      */
	} else {
		tasks_waiting = OS_FALSE;	/* No                                       */
	}
	switch (opt) {
	case OS_DEL_NO_PEND:	/*û������ȴ�ʱ��ɾ���¼���־�� Delete group if no task waiting          */
		if (tasks_waiting == OS_FALSE) {    //ȷʵû������ȴ�
#if OS_FLAG_NAME_EN > 0u
			pgrp->OSFlagName = (INT8U *) (void *) "?";  //�¼���־�����ֻ�ԭ
#endif
			pgrp->OSFlagType = OS_EVENT_TYPE_UNUSED;        //���͸�Ϊδʹ��
			pgrp->OSFlagWaitList = (void *) OSFlagFreeList;	/*���¼���־��黹�������¼���־����� Return group to free list           */
			pgrp->OSFlagFlags = (OS_FLAGS) 0;               //��ǰ�¼���־״̬����
			OSFlagFreeList = pgrp;                          //�����¼���־�����ָ�����
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_NONE;
			pgrp_return = (OS_FLAG_GRP *) 0;	/* Event Flag Group has been deleted        */
		} else {                            //������ȴ�������ɾ��
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_TASK_WAITING;
			pgrp_return = pgrp;
		}
		break;

	case OS_DEL_ALWAYS:	/*������û������ȴ�����ɾ���¼���־��ָ�� Always delete the event flag group       */
		pnode = (OS_FLAG_NODE *) pgrp->OSFlagWaitList;  //��ȡ�ȴ������е�����
		while (pnode != (OS_FLAG_NODE *) 0) {	/*�������еȴ������е����� Ready ALL tasks waiting for flags        */
			(void) OS_FlagTaskRdy(pnode, (OS_FLAGS) 0); //��Ϊ����̬
			pnode = (OS_FLAG_NODE *) pnode->OSFlagNodeNext; //ָ��ȴ����е���һ��ָ��
		}
#if OS_FLAG_NAME_EN > 0u
		pgrp->OSFlagName = (INT8U *) (void *) "?";  //�¼���־�����ֻ�ԭ
#endif
		pgrp->OSFlagType = OS_EVENT_TYPE_UNUSED;        //���͸�Ϊδʹ��
		pgrp->OSFlagWaitList = (void *) OSFlagFreeList;	/*���¼���־��黹�������¼���־����� Return group to free list                */
		pgrp->OSFlagFlags = (OS_FLAGS) 0;               //��ǰ�¼���־״̬����
		OSFlagFreeList = pgrp;                          //�����¼���־�����ָ�����
		OS_EXIT_CRITICAL();
		if (tasks_waiting == OS_TRUE) {	/*������ת���˾���̬ Reschedule only if task(s) were waiting  */
			OS_Sched();	/*���µ��� Find highest priority task ready to run  */
		}
		*perr = OS_ERR_NONE;
		pgrp_return = (OS_FLAG_GRP *) 0;	/* Event Flag Group has been deleted        */
		break;

	default:                //�����������
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_INVALID_OPT;
		pgrp_return = pgrp;
		break;
	}
	return (pgrp_return);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                 GET THE NAME OF AN EVENT FLAG GROUP
*
* Description: This function is used to obtain the name assigned to an event flag group
*              ��ȡ�¼���־�������
*
* Arguments  : pgrp      is a pointer to the event flag group.
*                        ָ���¼���־��ĵ�ָ��
*
*              pname     is pointer to a pointer to an ASCII string that will receive the name of the event flag
*                        group.
*                        �����ѯ��������
*
*              perr      is a pointer to an error code that can contain one of the following values:
*
*                        OS_ERR_NONE                if the requested task is resumed
*                        OS_ERR_EVENT_TYPE          if 'pevent' is not pointing to an event flag group
*                        OS_ERR_PNAME_NULL          You passed a NULL pointer for 'pname'
*                        OS_ERR_FLAG_INVALID_PGRP   if you passed a NULL pointer for 'pgrp'
*                        OS_ERR_NAME_GET_ISR        if you called this function from an ISR
*
* Returns    : The length of the string or 0 if the 'pgrp' is a NULL pointer.
*********************************************************************************************************
*/

#if OS_FLAG_NAME_EN > 0u
INT8U OSFlagNameGet(OS_FLAG_GRP * pgrp, INT8U ** pname, INT8U * perr)
{
	INT8U len;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pgrp == (OS_FLAG_GRP *) 0) {	/*ȷ��������¼���־��ָ��Ϸ� Is 'pgrp' a NULL pointer?                          */
		*perr = OS_ERR_FLAG_INVALID_PGRP;
		return (0u);
	}
	if (pname == (INT8U **) 0) {	/*ȷ���������ֿռ���� Is 'pname' a NULL pointer?                         */
		*perr = OS_ERR_PNAME_NULL;
		return (0u);
	}
#endif
	if (OSIntNesting > 0u) {	/*ȷ���������ж��е��� See if trying to call from an ISR                  */
		*perr = OS_ERR_NAME_GET_ISR;
		return (0u);
	}
	OS_ENTER_CRITICAL();
	if (pgrp->OSFlagType != OS_EVENT_TYPE_FLAG) {   //ȷ��������ȷ��Ϊ�¼���־������
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_EVENT_TYPE;
		return (0u);
	}
	*pname = pgrp->OSFlagName;                  //��ȡ�¼���־������
	len = OS_StrLen(*pname);                    //�������ֵĳ���
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
	return (len);
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                 ASSIGN A NAME TO AN EVENT FLAG GROUP
*
* Description: This function assigns a name to an event flag group.
*              �����¼���־�������
*
* Arguments  : pgrp      is a pointer to the event flag group.
*                        ָ���¼���־���ָ��
*
*              pname     is a pointer to an ASCII string that will be used as the name of the event flag
*                        group.
*                        ��Ҫ���õ��¼���־�������
*
*              perr      is a pointer to an error code that can contain one of the following values:
*
*                        OS_ERR_NONE                if the requested task is resumed
*                        OS_ERR_EVENT_TYPE          if 'pevent' is not pointing to an event flag group
*                        OS_ERR_PNAME_NULL          You passed a NULL pointer for 'pname'
*                        OS_ERR_FLAG_INVALID_PGRP   if you passed a NULL pointer for 'pgrp'
*                        OS_ERR_NAME_SET_ISR        if you called this function from an ISR
*
* Returns    : None
*********************************************************************************************************
*/

#if OS_FLAG_NAME_EN > 0u
void OSFlagNameSet(OS_FLAG_GRP * pgrp, INT8U * pname, INT8U * perr)
{
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pgrp == (OS_FLAG_GRP *) 0) {	/*ȷ��������¼���־��ָ����� Is 'pgrp' a NULL pointer?                          */
		*perr = OS_ERR_FLAG_INVALID_PGRP;
		return;
	}
	if (pname == (INT8U *) 0) {	/*ȷ�����ַǿ� Is 'pname' a NULL pointer?                         */
		*perr = OS_ERR_PNAME_NULL;
		return;
	}
#endif
	if (OSIntNesting > 0u) {	/*ȷ���������ж��е��� See if trying to call from an ISR                  */
		*perr = OS_ERR_NAME_SET_ISR;
		return;
	}
	OS_ENTER_CRITICAL();
	if (pgrp->OSFlagType != OS_EVENT_TYPE_FLAG) {   //ȷ��������ȷ�����¼���־������
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_EVENT_TYPE;
		return;
	}
	pgrp->OSFlagName = pname;                       //���¼���־������
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
	return;
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                        WAIT ON AN EVENT FLAG GROUP
*
* Description: This function is called to wait for a combination of bits to be set in an event flag
*              group.  Your application can wait for ANY bit to be set or ALL bits to be set.
*              �����¼���־���е��¼���־λ
*
* Arguments  : pgrp          is a pointer to the desired event flag group.
*                            ָ���¼���־���ָ��
*
*              flags         Is a bit pattern indicating which bit(s) (i.e. flags) you wish to wait for.
*                            The bits you want are specified by setting the corresponding bits in
*                            'flags'.  e.g. if your application wants to wait for bits 0 and 1 then
*                            'flags' would contain 0x03.
*                            ָ����Ҫ�����¼���־λ������Ϊ1�������Ӧλ��
*
*              wait_type     specifies whether you want ALL bits to be set or ANY of the bits to be set.
*                            You can specify the following argument:
*                            ����ȴ��¼���־λ�ķ�ʽ
*
*                            OS_FLAG_WAIT_CLR_ALL   You will wait for ALL bits in 'mask' to be clear (0)
*                            OS_FLAG_WAIT_SET_ALL   You will wait for ALL bits in 'mask' to be set   (1)
*                            OS_FLAG_WAIT_CLR_ANY   You will wait for ANY bit  in 'mask' to be clear (0)
*                            OS_FLAG_WAIT_SET_ANY   You will wait for ANY bit  in 'mask' to be set   (1)
*
*                            NOTE: Add OS_FLAG_CONSUME if you want the event flag to be 'consumed' by
*                                  the call.  Example, to wait for any flag in a group AND then clear
*                                  the flags that are present, set 'wait_type' to:
*
*                                  OS_FLAG_WAIT_SET_ANY + OS_FLAG_CONSUME
*                                  ���ʹ����OS_FLAG_CONSUME������ʹ�����¼�����Ӧ���¼���־λ�ָ�
*
*              timeout       is an optional timeout (in clock ticks) that your task will wait for the
*                            desired bit combination.  If you specify 0, however, your task will wait
*                            forever at the specified event flag group or, until a message arrives.
*                            ��ʱ���壬��Ϊ0���ʾ���õȴ�
*
*              perr          is a pointer to an error code and can be:
*                            OS_ERR_NONE               The desired bits have been set within the specified
*                                                      'timeout'.
*                            OS_ERR_PEND_ISR           If you tried to PEND from an ISR
*                            OS_ERR_FLAG_INVALID_PGRP  If 'pgrp' is a NULL pointer.
*                            OS_ERR_EVENT_TYPE         You are not pointing to an event flag group
*                            OS_ERR_TIMEOUT            The bit(s) have not been set in the specified
*                                                      'timeout'.
*                            OS_ERR_PEND_ABORT         The wait on the flag was aborted.
*                            OS_ERR_FLAG_WAIT_TYPE     You didn't specify a proper 'wait_type' argument.
*
* Returns    : The flags in the event flag group that made the task ready or, 0 if a timeout or an error
*              occurred.
*
* Called from: Task ONLY
*
* Note(s)    : 1) IMPORTANT, the behavior of this function has changed from PREVIOUS versions.  The
*                 function NOW returns the flags that were ready INSTEAD of the current state of the
*                 event flags.
*********************************************************************************************************
*/

OS_FLAGS OSFlagPend(OS_FLAG_GRP * pgrp, OS_FLAGS flags, INT8U wait_type, INT32U timeout, INT8U * perr)
{
	OS_FLAG_NODE node;
	OS_FLAGS flags_rdy;
	INT8U result;
	INT8U pend_stat;
	BOOLEAN consume;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�����ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pgrp == (OS_FLAG_GRP *) 0) {	/*ȷ���¼���־��ָ����Ч Validate 'pgrp'                          */
		*perr = OS_ERR_FLAG_INVALID_PGRP;
		return ((OS_FLAGS) 0);
	}
#endif
	if (OSIntNesting > 0u) {	/*ȷ���������ն��е��� See if called from ISR ...               */
		*perr = OS_ERR_PEND_ISR;	/* ... can't PEND from an ISR               */
		return ((OS_FLAGS) 0);
	}
	if (OSLockNesting > 0u) {	/* ȷ��������ȹ���û������See if called with scheduler locked ...  */
		*perr = OS_ERR_PEND_LOCKED;	/* ... can't PEND when locked               */
		return ((OS_FLAGS) 0);
	}
	if (pgrp->OSFlagType != OS_EVENT_TYPE_FLAG) {	/*ȷ���¼���־��������ȷ Validate event block type                */
		*perr = OS_ERR_EVENT_TYPE;
		return ((OS_FLAGS) 0);
	}
	result = (INT8U) (wait_type & OS_FLAG_CONSUME); //����Ƿ�ʹ����OS_FLAG_CONSUME��־
	if (result != (INT8U) 0) {	/*���ʹ���� See if we need to consume the flags      */
		wait_type &= (INT8U) ~ (INT8U) OS_FLAG_CONSUME; //���ȴ������е�OS_FLAG_CONSUME��־���
		consume = OS_TRUE;                              //����consumeѡ��״̬��־
	} else {                    //���û��ʹ��OS_FALG_CONSUME
		consume = OS_FALSE;
	}
/*$PAGE*/
	OS_ENTER_CRITICAL();         //���ж�
	switch (wait_type) {
	case OS_FLAG_WAIT_SET_ALL:	/*���Ҫ��������Ҫ��λ��1 See if all required flags are set        */
		flags_rdy = (OS_FLAGS) (pgrp->OSFlagFlags & flags);	/*����Ҫ�����¼�λ��ȡ���� Extract only the bits we want     */
		if (flags_rdy == flags) {	/*��Щ��Ҫ��λ�ǲ��Ƕ���1?�Ƿ�����Ҫ��? Must match ALL the bits that we want     */
			if (consume == OS_TRUE) {	/*�Ƿ�����consume���ѱ�־ See if we need to consume the flags      */
				pgrp->OSFlagFlags &= (OS_FLAGS) ~ flags_rdy;	/*ʹ��������Щ��Ҫ���¼������� Clear ONLY the flags we wanted    */
			}
			OSTCBCur->OSTCBFlagsRdy = flags_rdy;	/*����Ҫ�����¼�λ���� Save flags that were ready               */
			OS_EXIT_CRITICAL();	/*���ж� Yes, condition met, return to caller     */
			*perr = OS_ERR_NONE;
			return (flags_rdy);
		} else {	/*���û�п��õ��¼���־�� Block task until events occur or timeout */
			OS_FlagBlock(pgrp, &node, flags, wait_type, timeout);  //�������߼ӵ��¼���־��ĵȴ������б��У���������
			OS_EXIT_CRITICAL();
		}
		break;

	case OS_FLAG_WAIT_SET_ANY:  //����ָ���ı�־λ��1����
		flags_rdy = (OS_FLAGS) (pgrp->OSFlagFlags & flags);	/*����Ҫ�����¼�λ��ȡ���� Extract only the bits we want    */
		if (flags_rdy != (OS_FLAGS) 0) {	/*ֻҪ��ȫ��0���� See if any flag set                      */
			if (consume == OS_TRUE) {	/*�Ƿ�����consume���ѱ�־ See if we need to consume the flags      */
				pgrp->OSFlagFlags &= (OS_FLAGS) ~ flags_rdy;	/*����Ӧ���¼���ʶλ��0 Clear ONLY the flags that we got */
			}
			OSTCBCur->OSTCBFlagsRdy = flags_rdy;	/*����Ҫ�����¼�λ���� Save flags that were ready               */
			OS_EXIT_CRITICAL();	/* Yes, condition met, return to caller     */
			*perr = OS_ERR_NONE;
			return (flags_rdy);
		} else {	/* Block task until events occur or timeout */
			OS_FlagBlock(pgrp, &node, flags, wait_type, timeout);   //���û�п��õ��¼���־�飬��Ҫ�ȵ��¼��������߳�ʱ
			OS_EXIT_CRITICAL();
		}
		break;

#if OS_FLAG_WAIT_CLR_EN > 0u
	case OS_FLAG_WAIT_CLR_ALL:	/*Ҫ�����е��¼���־λ������ See if all required flags are cleared    */
		flags_rdy = (OS_FLAGS) ~ pgrp->OSFlagFlags & flags;	/*����Ҫ�����¼�λ��ȡ��������Ҫ�Ƚ���ǰ��״̬ȡ�� Extract only the bits we want     */
		if (flags_rdy == flags) {	/*����Ƿ��п��õ��¼���־�� Must match ALL the bits that we want     */
			if (consume == OS_TRUE) {	/*�Ƿ�����consume���ѱ�־ See if we need to consume the flags      */
				pgrp->OSFlagFlags |= flags_rdy;	/*��ָ�����¼���־λ��λ���лָ� Set ONLY the flags that we wanted        */
			}
			OSTCBCur->OSTCBFlagsRdy = flags_rdy;	/*����Ҫ�����¼�λ���� Save flags that were ready               */
			OS_EXIT_CRITICAL();	/* Yes, condition met, return to caller     */
			*perr = OS_ERR_NONE;
			return (flags_rdy);
		} else {	/* Block task until events occur or timeout */
			OS_FlagBlock(pgrp, &node, flags, wait_type, timeout);   //���û�п��õ��¼���־�飬��Ҫ�ȵ��¼��������߳�ʱ
			OS_EXIT_CRITICAL();
		}
		break;

	case OS_FLAG_WAIT_CLR_ANY:                              //����ָ�����¼���־λ����
		flags_rdy = (OS_FLAGS) ~ pgrp->OSFlagFlags & flags;	/*��ȡ��Ҫ�ļ��λ Extract only the bits we want      */
		if (flags_rdy != (OS_FLAGS) 0) {	/*����Ƿ��п��õ��¼���־�� See if any flag cleared                  */
			if (consume == OS_TRUE) {	/*�Ƿ�����consume��־ See if we need to consume the flags      */
				pgrp->OSFlagFlags |= flags_rdy;	/*���ƶ����¼���־λ��λ���лָ� Set ONLY the flags that we got           */
			}
			OSTCBCur->OSTCBFlagsRdy = flags_rdy;	/* ����Ҫ�����¼�λ���� Save flags that were ready               */
			OS_EXIT_CRITICAL();	/* Yes, condition met, return to caller     */
			*perr = OS_ERR_NONE;
			return (flags_rdy);
		} else {	/* Block task until events occur or timeout */
			OS_FlagBlock(pgrp, &node, flags, wait_type, timeout);  //���û�п��õ��¼���־�飬��Ҫ�ȵ��¼��������߳�ʱ
			OS_EXIT_CRITICAL();
		}
		break;
#endif

	default:
		OS_EXIT_CRITICAL();
		flags_rdy = (OS_FLAGS) 0;
		*perr = OS_ERR_FLAG_WAIT_TYPE;          //wait_type����ָ���Ĳ���֮һ
		return (flags_rdy);
	}
/*$PAGE*/
	OS_Sched();		/*�����߱�������Ҫ���µ��� Find next HPT ready to run               */
	OS_ENTER_CRITICAL();    //�����ٽ��������ж�
	//���ٴη���ʱ����Ҫ�������Ϊ��ʱ������Ϊ�¼��������ָ������ߵ�����
	if (OSTCBCur->OSTCBStatPend != OS_STAT_PEND_OK) {	/*�������Ϊ��ʱ������ȡ��������ָ����� Have we timed-out or aborted?            */
		pend_stat = OSTCBCur->OSTCBStatPend;            //���浱ǰ���������״̬
		OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;      //��ǰ���������̬��־����
		OS_FlagUnlink(&node);                           //��������¼���־�ȴ�������ȡ��
		OSTCBCur->OSTCBStat = OS_STAT_RDY;	/*�����־��Ϊ����̬ Yes, make task ready-to-run              */
		OS_EXIT_CRITICAL();
		flags_rdy = (OS_FLAGS) 0;       //û�л�ȡ���¼���־������0
		switch (pend_stat) {            //�鿴�����߻ָ��ľ���ԭ��
		case OS_STAT_PEND_ABORT:        //��Ϊȡ������
			*perr = OS_ERR_PEND_ABORT;	/* Indicate that we aborted   waiting       */
			break;

		case OS_STAT_PEND_TO:           //��Ϊ��ʱ
		default:
			*perr = OS_ERR_TIMEOUT;	/* Indicate that we timed-out waiting       */
			break;
		}
		return (flags_rdy);
	}
    //����Ϊ�¼��������ָ�������
	flags_rdy = OSTCBCur->OSTCBFlagsRdy;    //����Ҫ���¼���־����
	if (consume == OS_TRUE) {	/*consume���Ѵ��� See if we need to consume the flags      */
		switch (wait_type) {
		case OS_FLAG_WAIT_SET_ALL:
		case OS_FLAG_WAIT_SET_ANY:	/* Clear ONLY the flags we got              */
			pgrp->OSFlagFlags &= (OS_FLAGS) ~ flags_rdy;
			break;

#if OS_FLAG_WAIT_CLR_EN > 0u
		case OS_FLAG_WAIT_CLR_ALL:
		case OS_FLAG_WAIT_CLR_ANY:	/* Set   ONLY the flags we got              */
			pgrp->OSFlagFlags |= flags_rdy;
			break;
#endif
		default:
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_FLAG_WAIT_TYPE;
			return ((OS_FLAGS) 0);
		}
	}
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;	/* Event(s) must have occurred              */
	return (flags_rdy);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                               GET FLAGS WHO CAUSED TASK TO BECOME READY
*
* Description: This function is called to obtain the flags that caused the task to become ready to run.
*              In other words, this function allows you to tell "Who done it!".
*              �鿴������Ҫ���¼���־���λ
*
* Arguments  : None
*
* Returns    : The flags that caused the task to be ready.
*
* Called from: Task ONLY
*********************************************************************************************************
*/

OS_FLAGS OSFlagPendGetFlagsRdy(void)
{
	OS_FLAGS flags;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register */
	OS_CPU_SR cpu_sr = 0u;      //�����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif



	OS_ENTER_CRITICAL();
	flags = OSTCBCur->OSTCBFlagsRdy;    //��ȡ��Ҫ���¼���־���λ����Ϊ�������¼���־������У�OSTCBCur->OSTCBFlagsRdy = flags_rdy;
	OS_EXIT_CRITICAL();
	return (flags);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                         POST EVENT FLAG BIT(S)
*
* Description: This function is called to set or clear some bits in an event flag group.  The bits to
*              set or clear are specified by a 'bit mask'.
*              ��λ�������¼���־���е��¼���־λ
*
* Arguments  : pgrp          is a pointer to the desired event flag group.
*                            ָ���¼���־���ָ��
*
*              flags         If 'opt' (see below) is OS_FLAG_SET, each bit that is set in 'flags' will
*                            set the corresponding bit in the event flag group.  e.g. to set bits 0, 4
*                            and 5 you would set 'flags' to:
*
*                                0x31     (note, bit 0 is least significant bit)
*
*                            If 'opt' (see below) is OS_FLAG_CLR, each bit that is set in 'flags' will
*                            CLEAR the corresponding bit in the event flag group.  e.g. to clear bits 0,
*                            4 and 5 you would specify 'flags' as:
*
*                                0x31     (note, bit 0 is least significant bit)
*                            ָ����Ҫ������¼���־λ
*
*              opt           indicates whether the flags will be:
*                                set     (OS_FLAG_SET) or
*                                cleared (OS_FLAG_CLR)
*                            ָ���¼���־λ�����÷�ʽ����λor����
*
*              perr          is a pointer to an error code and can be:
*                            OS_ERR_NONE                The call was successfull
*                            OS_ERR_FLAG_INVALID_PGRP   You passed a NULL pointer
*                            OS_ERR_EVENT_TYPE          You are not pointing to an event flag group
*                            OS_ERR_FLAG_INVALID_OPT    You specified an invalid option
*
* Returns    : the new value of the event flags bits that are still set.
*
* Called From: Task or ISR
*
* WARNING(s) : 1) The execution time of this function depends on the number of tasks waiting on the event
*                 flag group.
*              2) The amount of time interrupts are DISABLED depends on the number of tasks waiting on
*                 the event flag group.
*********************************************************************************************************
*/
OS_FLAGS OSFlagPost(OS_FLAG_GRP * pgrp, OS_FLAGS flags, INT8U opt, INT8U * perr)
{
	OS_FLAG_NODE *pnode;
	BOOLEAN sched;
	OS_FLAGS flags_cur;
	OS_FLAGS flags_rdy;
	BOOLEAN rdy;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register       */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pgrp == (OS_FLAG_GRP *) 0) {	/*ȷ��������¼���־��ָ����� Validate 'pgrp'                                */
		*perr = OS_ERR_FLAG_INVALID_PGRP;
		return ((OS_FLAGS) 0);
	}
#endif
	if (pgrp->OSFlagType != OS_EVENT_TYPE_FLAG) {	/*ȷ���¼��������¼���־������ Make sure we are pointing to an event flag grp */
		*perr = OS_ERR_EVENT_TYPE;
		return ((OS_FLAGS) 0);
	}
/*$PAGE*/
	OS_ENTER_CRITICAL();
	switch (opt) {
	case OS_FLAG_CLR:                               //���¼���־���е�ָ��λ����
		pgrp->OSFlagFlags &= (OS_FLAGS) ~ flags;	/* Clear the flags specified in the group         */
		break;

	case OS_FLAG_SET:                               //���¼���־���е�ָ��λ��1
		pgrp->OSFlagFlags |= flags;	/* Set   the flags specified in the group         */
		break;

	default:                                        //�����������
		OS_EXIT_CRITICAL();	/* INVALID option                                 */
		*perr = OS_ERR_FLAG_INVALID_OPT;
		return ((OS_FLAGS) 0);
	}
	sched = OS_FALSE;	/* �ٶ����¼���־�Ĳ������ᵼ�¸������ȼ�������� Indicate that we don't need rescheduling       */
	pnode = (OS_FLAG_NODE *) pgrp->OSFlagWaitList;  //��ȡ�ȴ��¼���־�Ľڵ�
	while (pnode != (OS_FLAG_NODE *) 0) {	/*�����¼���־��ĵȴ������б� Go through all tasks waiting on event flag(s)  */
		switch (pnode->OSFlagNodeWaitType) {//���ÿһ���ȴ��¼���־������
		case OS_FLAG_WAIT_SET_ALL:	/*Ҫ����Ҫ�ı�־ȫ��Ϊ1 See if all req. flags are set for current node */
			flags_rdy = (OS_FLAGS) (pgrp->OSFlagFlags & pnode->OSFlagNodeFlags);//��ȡ��Ҫ���¼���־λ�ĵ�ǰ״̬
			if (flags_rdy == pnode->OSFlagNodeFlags) {     //��ǰ���¼���־λ��״̬����Ҫ��
				rdy = OS_FlagTaskRdy(pnode, flags_rdy);	/*���ȴ��¼���־�������ת����� Make task RTR, event(s) Rx'd          */
				if (rdy == OS_TRUE) {   //ת������ɹ�
					sched = OS_TRUE;	/*���ȱ�־��Ϊ�� When done we will reschedule          */
				}
			}
			break;          //�����������һ���ڵ�

		case OS_FLAG_WAIT_SET_ANY:	/*Ҫ������һ��Ϊ1 See if any flag set                            */
			flags_rdy = (OS_FLAGS) (pgrp->OSFlagFlags & pnode->OSFlagNodeFlags); //��ȡ��Ҫ���¼���־λ�ĵ�ǰ״̬
			if (flags_rdy != (OS_FLAGS) 0) {    //��ǰ���¼���־���״̬����Ҫ�󣬲�Ϊ�㣬����һ��Ϊ1
				rdy = OS_FlagTaskRdy(pnode, flags_rdy);	/* ���ȴ��¼���־�������ת�����̬Make task RTR, event(s) Rx'd          */
				if (rdy == OS_TRUE) {   //ת������ɹ�
					sched = OS_TRUE;	/*���ȱ�־��Ϊ�� When done we will reschedule          */
				}
			}
			break;          //�����������һ���ڵ�

#if OS_FLAG_WAIT_CLR_EN > 0u
		case OS_FLAG_WAIT_CLR_ALL:	/*Ҫ����Ҫ�ı�־ȫ��Ϊ0 See if all req. flags are set for current node */
			flags_rdy = (OS_FLAGS) ~ pgrp->OSFlagFlags & pnode->OSFlagNodeFlags; //��ȡ��Ҫ���¼���־λ�ĵ�ǰ״̬����ΪҪ��Ϊ0������ȡ��
			if (flags_rdy == pnode->OSFlagNodeFlags) {  //��ǰ���¼���־���״̬����Ҫ��
				rdy = OS_FlagTaskRdy(pnode, flags_rdy);	/*���ȴ��¼���־�������ת�����̬ Make task RTR, event(s) Rx'd          */
				if (rdy == OS_TRUE) {   //ת������ɹ�
					sched = OS_TRUE;	/*���ȱ�־��Ϊ�� When done we will reschedule          */
				}
			}
			break;          //�����������һ���ڵ�

		case OS_FLAG_WAIT_CLR_ANY:	/*Ҫ����Ҫ�ı�־����һ��Ϊ0 See if any flag set                            */
			flags_rdy = (OS_FLAGS) ~ pgrp->OSFlagFlags & pnode->OSFlagNodeFlags; //��ȡ��Ҫ���¼���־λ�ĵ�ǰ״̬����ΪҪ��Ϊ0������ȡ��
			if (flags_rdy != (OS_FLAGS) 0) {  //��ǰ���¼���־���״̬����Ҫ��
				rdy = OS_FlagTaskRdy(pnode, flags_rdy);	/*���ȴ��¼���־�������ת�����̬ Make task RTR, event(s) Rx'd          */
				if (rdy == OS_TRUE) {   //ת������ɹ�
					sched = OS_TRUE;	/*���ȱ�־��Ϊ�� When done we will reschedule          */
				}
			}
			break;          //�����������һ���ڵ�
#endif
		default:            //�����������
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_FLAG_WAIT_TYPE;
			return ((OS_FLAGS) 0);
		}
		pnode = (OS_FLAG_NODE *) pnode->OSFlagNodeNext;	/*ͨ����������һ���ȴ��ڵ�ָ�� Point to next task waiting for event flag(s) */
	}
	OS_EXIT_CRITICAL();
	if (sched == OS_TRUE) { //���ȱ�־Ϊ�棬˵���ڱ����Ĺ�����������ת�����̬
		OS_Sched();
	}
	OS_ENTER_CRITICAL();
	flags_cur = pgrp->OSFlagFlags; //���ص�ǰ�¼���־λ��״̬
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
	return (flags_cur);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                           QUERY EVENT FLAG
*
* Description: This function is used to check the value of the event flag group.
*              ��ѯ�¼���־���״̬
*
* Arguments  : pgrp         is a pointer to the desired event flag group.
*                           ָ���¼���־���ָ��
*
*              perr          is a pointer to an error code returned to the called:
*                            OS_ERR_NONE                The call was successfull
*                            OS_ERR_FLAG_INVALID_PGRP   You passed a NULL pointer
*                            OS_ERR_EVENT_TYPE          You are not pointing to an event flag group
*
* Returns    : The current value of the event flag group.�����¼���־����¼���־״̬
*
* Called From: Task or ISR
*********************************************************************************************************
*/

#if OS_FLAG_QUERY_EN > 0u
OS_FLAGS OSFlagQuery(OS_FLAG_GRP * pgrp, INT8U * perr)
{
	OS_FLAGS flags;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register          */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pgrp == (OS_FLAG_GRP *) 0) {	/*ȷ���¼���־��ָ�����  Validate 'pgrp'                                   */
		*perr = OS_ERR_FLAG_INVALID_PGRP;
		return ((OS_FLAGS) 0);
	}
#endif
	if (pgrp->OSFlagType != OS_EVENT_TYPE_FLAG) {	/*ȷ��������ȷ Validate event block type                         */
		*perr = OS_ERR_EVENT_TYPE;
		return ((OS_FLAGS) 0);
	}
	OS_ENTER_CRITICAL();
	flags = pgrp->OSFlagFlags;                      //��ȡ��ǰ�¼���־״̬
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
	return (flags);		/* Return the current value of the event flags       */
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                         SUSPEND TASK UNTIL EVENT FLAG(s) RECEIVED OR TIMEOUT OCCURS
*
* Description: This function is internal to uC/OS-II and is used to put a task to sleep until the desired
*              event flag bit(s) are set.
*              ����ǰ�¼�����ȴ��¼���־��ʱ
*
* Arguments  : pgrp          is a pointer to the desired event flag group.
*                            ������¼���־��
*
*              pnode         is a pointer to a structure which contains data about the task waiting for
*                            event flag bit(s) to be set.
*                            Ϊ��ǰ�������ĵȴ��¼���־�ڵ�
*
*              flags         Is a bit pattern indicating which bit(s) (i.e. flags) you wish to check.
*                            The bits you want are specified by setting the corresponding bits in
*                            'flags'.  e.g. if your application wants to wait for bits 0 and 1 then
*                            'flags' would contain 0x03.
*                            ��Ҫ�����¼���־λ
*
*              wait_type     specifies whether you want ALL bits to be set/cleared or ANY of the bits
*                            to be set/cleared.
*                            You can specify the following argument:
*
*                            OS_FLAG_WAIT_CLR_ALL   You will check ALL bits in 'mask' to be clear (0)
*                            OS_FLAG_WAIT_CLR_ANY   You will check ANY bit  in 'mask' to be clear (0)
*                            OS_FLAG_WAIT_SET_ALL   You will check ALL bits in 'mask' to be set   (1)
*                            OS_FLAG_WAIT_SET_ANY   You will check ANY bit  in 'mask' to be set   (1)
*                            ��������
*
*              timeout       is the desired amount of time that the task will wait for the event flag
*                            bit(s) to be set.
*                            ��ʱ�¼�
*
* Returns    : none
*
* Called by  : OSFlagPend()  OS_FLAG.C
*
* Note(s)    : This function is INTERNAL to uC/OS-II and your application should not call it.
*********************************************************************************************************
*/

static void OS_FlagBlock(OS_FLAG_GRP * pgrp, OS_FLAG_NODE * pnode, OS_FLAGS flags, INT8U wait_type, INT32U timeout)
{
	OS_FLAG_NODE *pnode_next;
	INT8U y;


	OSTCBCur->OSTCBStat |= OS_STAT_FLAG;            //״̬��Ϊ�ȴ��¼���־��
	OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;      //����״̬
	OSTCBCur->OSTCBDly = timeout;	/*����ȴ�ʱ�� Store timeout in task's TCB                   */
#if OS_TASK_DEL_EN > 0u
	OSTCBCur->OSTCBFlagNode = pnode;	/*����TCBָ���¼���־�ڵ� TCB to link to node                           */
#endif
	pnode->OSFlagNodeFlags = flags;	/* ����ȴ����¼���־λ Save the flags that we need to wait for       */
	pnode->OSFlagNodeWaitType = wait_type;	/*����ȴ������� Save the type of wait we are doing            */
	pnode->OSFlagNodeTCB = (void *) OSTCBCur;	/*�¼���־�ڵ�ָ������TCB Link to task's TCB                            */
	pnode->OSFlagNodeNext = pgrp->OSFlagWaitList;	/*����ȴ����� Add node at beginning of event flag wait list */
	pnode->OSFlagNodePrev = (void *) 0;             //���ó�Ϊ�ȴ����еĶ���
	pnode->OSFlagNodeFlagGrp = (void *) pgrp;	/*����ָ���¼���־�� Link to Event Flag Group                      */
	pnode_next = (OS_FLAG_NODE *) pgrp->OSFlagWaitList; //��ȡ֮ǰ�ĵȴ����еĶ��׽ڵ㣬����ǰ�ĵڶ����ȴ��¼����нڵ�
	if (pnode_next != (void *) 0) {	/* ����ǿգ�������ǰ�ȴ�������ֻ��һ���ڵ㣬���Ǹոռ�����Ǹ� Is this the first NODE to insert?             */
		pnode_next->OSFlagNodePrev = pnode;	/*�����Ϊ�գ������ڶ����ڵ��ǰ��ָ��ָ��ոռ���Ľڵ㣬���˫������ No, link in doubly linked list                */
	}
	pgrp->OSFlagWaitList = (void *) pnode;  //�¼���־��ĵȴ����еĶ���ָ�����

	y = OSTCBCur->OSTCBY;	/*����ǰ�����񣬴Ӿ�������ȥ�� Suspend current task until flag(s) received   */
	OSRdyTbl[y] &= (OS_PRIO) ~ OSTCBCur->OSTCBBitX;
	if (OSRdyTbl[y] == 0x00u) {
		OSRdyGrp &= (OS_PRIO) ~ OSTCBCur->OSTCBBitY;
	}
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                    INITIALIZE THE EVENT FLAG MODULE
*
* Description: This function is called by uC/OS-II to initialize the event flag module.  Your application
*              MUST NOT call this function.  In other words, this function is internal to uC/OS-II.
*              ��ʼ���¼���־�����
*
* Arguments  : none
*
* Returns    : none
*
* WARNING    : You MUST NOT call this function from your code.  This is an INTERNAL function to uC/OS-II.
*********************************************************************************************************
*/

void OS_FlagInit(void)
{
#if OS_MAX_FLAGS == 1u                                  //�¼���־�����ֻ��һ��
	OSFlagFreeList = (OS_FLAG_GRP *) & OSFlagTbl[0];	/*���¼���־����ж��з���ռ� Only ONE event flag group!      */
	OSFlagFreeList->OSFlagType = OS_EVENT_TYPE_UNUSED;  //�¼���־��״̬λδʹ��
	OSFlagFreeList->OSFlagWaitList = (void *) 0;        //û�еȴ�����
	OSFlagFreeList->OSFlagFlags = (OS_FLAGS) 0;         //û���¼���־λ״̬
#if OS_FLAG_NAME_EN > 0u
	OSFlagFreeList->OSFlagName = (INT8U *) "?";         //����Ĭ��
#endif
#endif

#if OS_MAX_FLAGS >= 2u                                  //�¼���־�鲻ֹһ��
	INT16U ix;
	INT16U ix_next;
	OS_FLAG_GRP *pgrp1;
	OS_FLAG_GRP *pgrp2;


	OS_MemClr((INT8U *) & OSFlagTbl[0], sizeof(OSFlagTbl));	/*���������¼���־����ж��еĿռ� Clear the flag group table      */
	for (ix = 0u; ix < (OS_MAX_FLAGS - 1u); ix++) {	/*�������ж����е�ÿ���¼���־�飬ʵ����û�а������һ�� Init. list of free EVENT FLAGS  */
		ix_next = ix + 1u;                          //��һ��������
		pgrp1 = &OSFlagTbl[ix];                     //��ǰ���¼���־��
		pgrp2 = &OSFlagTbl[ix_next];                //��һ���¼���־��
		pgrp1->OSFlagType = OS_EVENT_TYPE_UNUSED;   //��ǰ������Ϊδʹ��
		pgrp1->OSFlagWaitList = (void *) pgrp2;     //��ǰ���¼���־��ĵȴ�����ָ����һ���¼���־�飬��ɵ�������
#if OS_FLAG_NAME_EN > 0u
		pgrp1->OSFlagName = (INT8U *) (void *) "?";	/* ����Ĭ�� Unknown name                    */
#endif
	}
    //�����һ���¼���־�鴦��
	pgrp1 = &OSFlagTbl[ix];                     //��ȡ���һ���¼���־���ָ��
	pgrp1->OSFlagType = OS_EVENT_TYPE_UNUSED;   //������Ϊδʹ��
	pgrp1->OSFlagWaitList = (void *) 0;         //���������һ��������û���ˣ�����ָ��ָ��0
#if OS_FLAG_NAME_EN > 0u
	pgrp1->OSFlagName = (INT8U *) (void *) "?";	/*����Ĭ�� Unknown name                    */
#endif
	OSFlagFreeList = &OSFlagTbl[0];             //�¼���־��Ķ���ָ�����
#endif
}

/*$PAGE*/
/*
*********************************************************************************************************
*                              MAKE TASK READY-TO-RUN, EVENT(s) OCCURRED
*
* Description: This function is internal to uC/OS-II and is used to make a task ready-to-run because the
*              desired event flag bits have been set.
*              �����¼���־���������ȴ�������ת�����
*
* Arguments  : pnode         is a pointer to a structure which contains data about the task waiting for
*                            event flag bit(s) to be set.
*                            �ȴ����¼���־�ڵ�
*
*              flags_rdy     contains the bit pattern of the event flags that cause the task to become
*                            ready-to-run.
*                            ����������¼���־λ��״̬
*
* Returns    : OS_TRUE       If the task has been placed in the ready list and thus needs scheduling
*              OS_FALSE      The task is still not ready to run and thus scheduling is not necessary
*
* Called by  : OSFlagsPost() OS_FLAG.C
*
* Note(s)    : 1) This function assumes that interrupts are disabled.
*              2) This function is INTERNAL to uC/OS-II and your application should not call it.
*********************************************************************************************************
*/

static BOOLEAN OS_FlagTaskRdy(OS_FLAG_NODE * pnode, OS_FLAGS flags_rdy)
{
	OS_TCB *ptcb;
	BOOLEAN sched;


	ptcb = (OS_TCB *) pnode->OSFlagNodeTCB;	/*��ȡ�ȴ����¼���־�ڵ�����Ӧ������TCB Point to TCB of waiting task             */
	ptcb->OSTCBDly = 0u;                    //��ʱ��0
	ptcb->OSTCBFlagsRdy = flags_rdy;        //���ȴ����¼���־λ��������Ϊ����������¼���־λ
	ptcb->OSTCBStat &= (INT8U) ~ (INT8U) OS_STAT_FLAG;//ȡ���¼���־��ĵȴ���־
	ptcb->OSTCBStatPend = OS_STAT_PEND_OK;      //�����־����
	if (ptcb->OSTCBStat == OS_STAT_RDY) {	/*��ǰ�������̬��ô Task now ready?                          */
		OSRdyGrp |= ptcb->OSTCBBitY;	/*�������������б� Put task into ready list                 */
		OSRdyTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
		sched = OS_TRUE;                //���سɹ���־
	} else {        //����û�д��ھ���̬����Ϊ���ܲ�ֹ�ڵȴ��¼���־��
		sched = OS_FALSE;
	}
	OS_FlagUnlink(pnode);       //�ӵȴ��б���ժ��
	return (sched);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                  UNLINK EVENT FLAG NODE FROM WAITING LIST
*
* Description: This function is internal to uC/OS-II and is used to unlink an event flag node from a
*              list of tasks waiting for the event flag.
*              ���¼���־��ĵȴ�������ժ���¼���־�ڵ�
*
* Arguments  : pnode         is a pointer to a structure which contains data about the task waiting for
*                            event flag bit(s) to be set.
*                            �¼���־�ڵ�ָ��
*
* Returns    : none
*
* Called by  : OS_FlagTaskRdy() OS_FLAG.C
*              OSFlagPend()     OS_FLAG.C
*              OSTaskDel()      OS_TASK.C
*
* Note(s)    : 1) This function assumes that interrupts are disabled.
*              2) This function is INTERNAL to uC/OS-II and your application should not call it.
*********************************************************************************************************
*/

void OS_FlagUnlink(OS_FLAG_NODE * pnode)
{
#if OS_TASK_DEL_EN > 0u
	OS_TCB *ptcb;
#endif
	OS_FLAG_GRP *pgrp;
	OS_FLAG_NODE *pnode_prev;
	OS_FLAG_NODE *pnode_next;


	pnode_prev = (OS_FLAG_NODE *) pnode->OSFlagNodePrev;          //Ҫժ�½ڵ��ǰһ���ڵ�
	pnode_next = (OS_FLAG_NODE *) pnode->OSFlagNodeNext;          //Ҫժ�½ڵ����һ���ڵ�
	if (pnode_prev == (OS_FLAG_NODE *) 0) {	/*���ǰ��Ľڵ�Ϊ�գ���Ҫժ�µ��Ƕ��׽ڵ� Is it first node in wait list?      */
		pgrp = (OS_FLAG_GRP *) pnode->OSFlagNodeFlagGrp;    //��ȡ��Ӧ���¼���־��
		pgrp->OSFlagWaitList = (void *) pnode_next;	/*�¼���־��ĵȴ��б����ָ�����Ϊ��һ���ڵ�      Update list for new 1st node   */
		if (pnode_next != (OS_FLAG_NODE *) 0) {    //�����һ���ڵ����
			pnode_next->OSFlagNodePrev = (OS_FLAG_NODE *) 0;	/*����һ���ڵ��ǰ��ڵ�ָ����Ϊ�գ���Ϊ�������Ƕ��׽ڵ��� Link new 1st node PREV to NULL */
		}
	} else {		/*���ǰ��Ľڵ㲻Ϊ�գ���ζ��Ҫժ�µĲ��Ƕ��׽ڵ� No,  A node somewhere in the list   */
		pnode_prev->OSFlagNodeNext = pnode_next;	/*��ǰһ���ڵ�ĺ���ָ��ָ����һ���ڵ㣬������ǰ�ڵ�      Link around the node to unlink */
		if (pnode_next != (OS_FLAG_NODE *) 0) {	/* �����һ���ڵ����     Was this the LAST node?        */
			pnode_next->OSFlagNodePrev = pnode_prev;	/* ��һ���ڵ��ǰ��ָ���Ϊǰһ���ڵ㣬������ǰ�ڵ�   No, Link around current node   */
		}
	}
#if OS_TASK_DEL_EN > 0u
	ptcb = (OS_TCB *) pnode->OSFlagNodeTCB;     //��ȡ�ڵ��Ӧ������TCB
	ptcb->OSTCBFlagNode = (OS_FLAG_NODE *) 0;   //��ָ���¼���־�ڵ��ָ����Ϊ�գ���ζ�Žڵ��Ӧ������û����Ҫ���¼���־��
#endif
}
#endif
