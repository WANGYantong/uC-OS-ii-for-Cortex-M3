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
* If you plan on using  uC/OS-II  in a commercial product you need to contact Micri祄 to properly license
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
*              无等待的获取事件标志组中的事件标志
*
* Arguments  : pgrp          is a pointer to the desired event flag group.
*                            指向事件标志组的指针
*
*              flags         Is a bit pattern indicating which bit(s) (i.e. flags) you wish to check.
*                            The bits you want are specified by setting the corresponding bits in
*                            'flags'.  e.g. if your application wants to wait for bits 0 and 1 then
*                            'flags' would contain 0x03.
*                            需要检查的事件标志位，如果为1，则检查对应位，为0则忽略。
*
*              wait_type     specifies whether you want ALL bits to be set/cleared or ANY of the bits
*                            to be set/cleared.等待事件标志位的方式
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
*                                  OS_FLAG_CONSUME表示任务在获得所需要的事件标志位后要将其恢复
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
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pgrp == (OS_FLAG_GRP *) 0) {	/*确保事件标志组指针可用 Validate 'pgrp'                          */
		*perr = OS_ERR_FLAG_INVALID_PGRP;
		return ((OS_FLAGS) 0);
	}
#endif
	if (pgrp->OSFlagType != OS_EVENT_TYPE_FLAG) {	/*确保类型正确 Validate event block type                */
		*perr = OS_ERR_EVENT_TYPE;
		return ((OS_FLAGS) 0);
	}
	result = (INT8U) (wait_type & OS_FLAG_CONSUME); //提取输入类型的consume位
	if (result != (INT8U) 0) {	/*是否使用了consume See if we need to consume the flags      */
		wait_type &= ~OS_FLAG_CONSUME;      //如果是，将consume位清除
		consume = OS_TRUE;                  //标志使用了consume
	} else {
		consume = OS_FALSE;
	}
/*$PAGE*/
	*perr = OS_ERR_NONE;	/* Assume NO error until proven otherwise.  */
	OS_ENTER_CRITICAL();
	switch (wait_type) {
	case OS_FLAG_WAIT_SET_ALL:	/*需要所有的请求位置1 See if all required flags are set        */
		flags_rdy = (OS_FLAGS) (pgrp->OSFlagFlags & flags);	/*提取出需要的位 Extract only the bits we want   */
		if (flags_rdy == flags) {	/*是否符合要求，全部是1 Must match ALL the bits that we want     */
			if (consume == OS_TRUE) {	/*如果使用了consume See if we need to consume the flags      */
				pgrp->OSFlagFlags &= (OS_FLAGS) ~ flags_rdy;	/*恢复之前的状态 Clear ONLY the flags we wanted  */
			}
		} else {                            //不符合要求，没有获取到事件标志
			*perr = OS_ERR_FLAG_NOT_RDY;
		}
		OS_EXIT_CRITICAL();
		break;

	case OS_FLAG_WAIT_SET_ANY:  //需要至少一个请求位为1
		flags_rdy = (OS_FLAGS) (pgrp->OSFlagFlags & flags);	/*提取出需要的位 Extract only the bits we want   */
		if (flags_rdy != (OS_FLAGS) 0) {	/*是否符合要求，不全为0，至少一位是1 See if any flag set                      */
			if (consume == OS_TRUE) {	/*如果使用了consume See if we need to consume the flags      */
				pgrp->OSFlagFlags &= (OS_FLAGS) ~ flags_rdy;	/*恢复之前的状态 Clear ONLY the flags we got     */
			}
		} else {                            //不符合要求，没有获取到事件标志
			*perr = OS_ERR_FLAG_NOT_RDY;
		}
		OS_EXIT_CRITICAL();
		break;

#if OS_FLAG_WAIT_CLR_EN > 0u
	case OS_FLAG_WAIT_CLR_ALL:	/*需要所有的请求位为0 See if all required flags are cleared    */
		flags_rdy = (OS_FLAGS) ~ pgrp->OSFlagFlags & flags;	/*提取出需要的位，这里要取反变成1 Extract only the bits we want     */
		if (flags_rdy == flags) {	/*是否符合要求，所有的取反后为1 Must match ALL the bits that we want     */
			if (consume == OS_TRUE) {	/*如果使用了consume See if we need to consume the flags      */
				pgrp->OSFlagFlags |= flags_rdy;	/*恢复之前的状态 Set ONLY the flags that we wanted        */
			}
		} else {                            //不符合要求，没有获取到事件标志
			*perr = OS_ERR_FLAG_NOT_RDY;
		}
		OS_EXIT_CRITICAL();
		break;

	case OS_FLAG_WAIT_CLR_ANY:  //需要至少一个请求位为0
		flags_rdy = (OS_FLAGS) ~ pgrp->OSFlagFlags & flags;	/*提取出需要的位，这里取反变成1 Extract only the bits we want      */
		if (flags_rdy != (OS_FLAGS) 0) {	/*是否符合要求，取反后至少一位为1 See if any flag cleared                  */
			if (consume == OS_TRUE) {	/*如果使用了consume See if we need to consume the flags      */
				pgrp->OSFlagFlags |= flags_rdy;	/*恢复之前的状态 Set ONLY the flags that we got           */
			}
		} else {                            //不符合要求，没有获取到事件标志
			*perr = OS_ERR_FLAG_NOT_RDY;
		}
		OS_EXIT_CRITICAL();
		break;
#endif

	default:                        //参数不合法
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
*              建立事件标志组
*
* Arguments  : flags         Contains the initial value to store in the event flag group.  事件标志初始值
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
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
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

	if (OSIntNesting > 0u) {	/*不能在中断服务函数中提供 See if called from ISR ...                      */
		*perr = OS_ERR_CREATE_ISR;	/* ... can't CREATE from an ISR                    */
		return ((OS_FLAG_GRP *) 0);
	}
	OS_ENTER_CRITICAL();
	pgrp = OSFlagFreeList;	/*从空闲事件标志组链表中取出一个 Get next free event flag                        */
	if (pgrp != (OS_FLAG_GRP *) 0) {	/*取下的是可用的 See if we have event flag groups available      */
		/* Adjust free list                                */
		OSFlagFreeList = (OS_FLAG_GRP *) OSFlagFreeList->OSFlagWaitList;  //调整空闲事件标志组链表的首指针
		pgrp->OSFlagType = OS_EVENT_TYPE_FLAG;	/*设置事件标志组类型 Set to event flag group type                    */
		pgrp->OSFlagFlags = flags;	/*设置初始值 Set to desired initial value                    */
		pgrp->OSFlagWaitList = (void *) 0;	/*清空等待任务列表 Clear list of tasks waiting on flags            */
#if OS_FLAG_NAME_EN > 0u
		pgrp->OSFlagName = (INT8U *) (void *) "?";  //名字
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
*              删除事件标志组，并将阻塞的任务置为就绪态
*
* Arguments  : pgrp          is a pointer to the desired event flag group.
*                            指向事件标志组的指针
*
*              opt           determines delete options as follows:指明删除条件
*                            opt == OS_DEL_NO_PEND   Deletes the event flag group ONLY if no task pending
*                                                    在没有任务等待的时候删除事件标志组
*                            opt == OS_DEL_ALWAYS    Deletes the event flag group even if tasks are
*                                                    waiting.  In this case, all the tasks pending will be
*                                                    readied.
*                                                    无论有没有任务在等待都删除事件标志组，并将等待的任务就绪
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
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pgrp == (OS_FLAG_GRP *) 0) {	/*确保输入的事件标志组可用 Validate 'pgrp'                          */
		*perr = OS_ERR_FLAG_INVALID_PGRP;
		return (pgrp);
	}
#endif
	if (OSIntNesting > 0u) {	/*确保不在中断服务函数中调用 See if called from ISR ...               */
		*perr = OS_ERR_DEL_ISR;	/* ... can't DELETE from an ISR             */
		return (pgrp);
	}
	if (pgrp->OSFlagType != OS_EVENT_TYPE_FLAG) {	/*确保类型是事件标志类型 Validate event group type                */
		*perr = OS_ERR_EVENT_TYPE;
		return (pgrp);
	}
	OS_ENTER_CRITICAL();
	if (pgrp->OSFlagWaitList != (void *) 0) {	/*等待队列中有没有任务 See if any tasks waiting on event flags  */
		tasks_waiting = OS_TRUE;	/* Yes                                      */
	} else {
		tasks_waiting = OS_FALSE;	/* No                                       */
	}
	switch (opt) {
	case OS_DEL_NO_PEND:	/*没有任务等待时才删除事件标志组 Delete group if no task waiting          */
		if (tasks_waiting == OS_FALSE) {    //确实没有任务等待
#if OS_FLAG_NAME_EN > 0u
			pgrp->OSFlagName = (INT8U *) (void *) "?";  //事件标志组名字还原
#endif
			pgrp->OSFlagType = OS_EVENT_TYPE_UNUSED;        //类型改为未使用
			pgrp->OSFlagWaitList = (void *) OSFlagFreeList;	/*将事件标志组归还到空闲事件标志组队列 Return group to free list           */
			pgrp->OSFlagFlags = (OS_FLAGS) 0;               //当前事件标志状态清零
			OSFlagFreeList = pgrp;                          //空闲事件标志组队首指针调整
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_NONE;
			pgrp_return = (OS_FLAG_GRP *) 0;	/* Event Flag Group has been deleted        */
		} else {                            //有任务等待，不能删除
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_TASK_WAITING;
			pgrp_return = pgrp;
		}
		break;

	case OS_DEL_ALWAYS:	/*不管有没有任务等待，都删除事件标志组指针 Always delete the event flag group       */
		pnode = (OS_FLAG_NODE *) pgrp->OSFlagWaitList;  //获取等待队列中的任务
		while (pnode != (OS_FLAG_NODE *) 0) {	/*遍历所有等待队列中的任务 Ready ALL tasks waiting for flags        */
			(void) OS_FlagTaskRdy(pnode, (OS_FLAGS) 0); //置为就绪态
			pnode = (OS_FLAG_NODE *) pnode->OSFlagNodeNext; //指向等待队列的下一个指针
		}
#if OS_FLAG_NAME_EN > 0u
		pgrp->OSFlagName = (INT8U *) (void *) "?";  //事件标志组名字还原
#endif
		pgrp->OSFlagType = OS_EVENT_TYPE_UNUSED;        //类型改为未使用
		pgrp->OSFlagWaitList = (void *) OSFlagFreeList;	/*将事件标志组归还到空闲事件标志组队列 Return group to free list                */
		pgrp->OSFlagFlags = (OS_FLAGS) 0;               //当前事件标志状态清零
		OSFlagFreeList = pgrp;                          //空闲事件标志组队首指针调整
		OS_EXIT_CRITICAL();
		if (tasks_waiting == OS_TRUE) {	/*有任务转入了就绪态 Reschedule only if task(s) were waiting  */
			OS_Sched();	/*重新调度 Find highest priority task ready to run  */
		}
		*perr = OS_ERR_NONE;
		pgrp_return = (OS_FLAG_GRP *) 0;	/* Event Flag Group has been deleted        */
		break;

	default:                //输入参数错误
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
*              获取事件标志组的名字
*
* Arguments  : pgrp      is a pointer to the event flag group.
*                        指向事件标志组的的指针
*
*              pname     is pointer to a pointer to an ASCII string that will receive the name of the event flag
*                        group.
*                        保存查询到的名字
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
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pgrp == (OS_FLAG_GRP *) 0) {	/*确保输入的事件标志组指针合法 Is 'pgrp' a NULL pointer?                          */
		*perr = OS_ERR_FLAG_INVALID_PGRP;
		return (0u);
	}
	if (pname == (INT8U **) 0) {	/*确保保存名字空间可用 Is 'pname' a NULL pointer?                         */
		*perr = OS_ERR_PNAME_NULL;
		return (0u);
	}
#endif
	if (OSIntNesting > 0u) {	/*确保不是在中断中调用 See if trying to call from an ISR                  */
		*perr = OS_ERR_NAME_GET_ISR;
		return (0u);
	}
	OS_ENTER_CRITICAL();
	if (pgrp->OSFlagType != OS_EVENT_TYPE_FLAG) {   //确保类型正确，为事件标志组类型
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_EVENT_TYPE;
		return (0u);
	}
	*pname = pgrp->OSFlagName;                  //获取事件标志组名字
	len = OS_StrLen(*pname);                    //返回名字的长度
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
*              设置事件标志组的名字
*
* Arguments  : pgrp      is a pointer to the event flag group.
*                        指向事件标志组的指针
*
*              pname     is a pointer to an ASCII string that will be used as the name of the event flag
*                        group.
*                        将要设置的事件标志组的名字
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
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pgrp == (OS_FLAG_GRP *) 0) {	/*确保输入的事件标志组指针可用 Is 'pgrp' a NULL pointer?                          */
		*perr = OS_ERR_FLAG_INVALID_PGRP;
		return;
	}
	if (pname == (INT8U *) 0) {	/*确保名字非空 Is 'pname' a NULL pointer?                         */
		*perr = OS_ERR_PNAME_NULL;
		return;
	}
#endif
	if (OSIntNesting > 0u) {	/*确保不是在中断中调用 See if trying to call from an ISR                  */
		*perr = OS_ERR_NAME_SET_ISR;
		return;
	}
	OS_ENTER_CRITICAL();
	if (pgrp->OSFlagType != OS_EVENT_TYPE_FLAG) {   //确保类型正确，是事件标志组类型
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_EVENT_TYPE;
		return;
	}
	pgrp->OSFlagName = pname;                       //给事件标志组命名
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
*              请求事件标志组中的事件标志位
*
* Arguments  : pgrp          is a pointer to the desired event flag group.
*                            指向事件标志组的指针
*
*              flags         Is a bit pattern indicating which bit(s) (i.e. flags) you wish to wait for.
*                            The bits you want are specified by setting the corresponding bits in
*                            'flags'.  e.g. if your application wants to wait for bits 0 and 1 then
*                            'flags' would contain 0x03.
*                            指向需要检查的事件标志位。若置为1，则检查对应位。
*
*              wait_type     specifies whether you want ALL bits to be set or ANY of the bits to be set.
*                            You can specify the following argument:
*                            定义等待事件标志位的方式
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
*                                  如果使用了OS_FLAG_CONSUME，则在使用了事件后将相应的事件标志位恢复
*
*              timeout       is an optional timeout (in clock ticks) that your task will wait for the
*                            desired bit combination.  If you specify 0, however, your task will wait
*                            forever at the specified event flag group or, until a message arrives.
*                            超时定义，置为0则表示永久等待
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
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pgrp == (OS_FLAG_GRP *) 0) {	/*确保事件标志组指针有效 Validate 'pgrp'                          */
		*perr = OS_ERR_FLAG_INVALID_PGRP;
		return ((OS_FLAGS) 0);
	}
#endif
	if (OSIntNesting > 0u) {	/*确保不是在终端中调用 See if called from ISR ...               */
		*perr = OS_ERR_PEND_ISR;	/* ... can't PEND from an ISR               */
		return ((OS_FLAGS) 0);
	}
	if (OSLockNesting > 0u) {	/* 确保任务调度功能没有上锁See if called with scheduler locked ...  */
		*perr = OS_ERR_PEND_LOCKED;	/* ... can't PEND when locked               */
		return ((OS_FLAGS) 0);
	}
	if (pgrp->OSFlagType != OS_EVENT_TYPE_FLAG) {	/*确保事件标志组类型正确 Validate event block type                */
		*perr = OS_ERR_EVENT_TYPE;
		return ((OS_FLAGS) 0);
	}
	result = (INT8U) (wait_type & OS_FLAG_CONSUME); //检查是否使用了OS_FLAG_CONSUME标志
	if (result != (INT8U) 0) {	/*如果使用了 See if we need to consume the flags      */
		wait_type &= (INT8U) ~ (INT8U) OS_FLAG_CONSUME; //将等待类型中的OS_FLAG_CONSUME标志清除
		consume = OS_TRUE;                              //设置consume选用状态标志
	} else {                    //如果没有使用OS_FALG_CONSUME
		consume = OS_FALSE;
	}
/*$PAGE*/
	OS_ENTER_CRITICAL();         //关中断
	switch (wait_type) {
	case OS_FLAG_WAIT_SET_ALL:	/*如果要求所有需要的位置1 See if all required flags are set        */
		flags_rdy = (OS_FLAGS) (pgrp->OSFlagFlags & flags);	/*将需要检测的事件位提取出来 Extract only the bits we want     */
		if (flags_rdy == flags) {	/*这些需要的位是不是都是1?是否满足要求? Must match ALL the bits that we want     */
			if (consume == OS_TRUE) {	/*是否定义了consume消费标志 See if we need to consume the flags      */
				pgrp->OSFlagFlags &= (OS_FLAGS) ~ flags_rdy;	/*使用完了这些需要的事件，清零 Clear ONLY the flags we wanted    */
			}
			OSTCBCur->OSTCBFlagsRdy = flags_rdy;	/*将需要检测的事件位保存 Save flags that were ready               */
			OS_EXIT_CRITICAL();	/*开中断 Yes, condition met, return to caller     */
			*perr = OS_ERR_NONE;
			return (flags_rdy);
		} else {	/*如果没有可用的事件标志组 Block task until events occur or timeout */
			OS_FlagBlock(pgrp, &node, flags, wait_type, timeout);  //将调用者加到事件标志组的等待任务列表中，挂起任务
			OS_EXIT_CRITICAL();
		}
		break;

	case OS_FLAG_WAIT_SET_ANY:  //任意指定的标志位置1即可
		flags_rdy = (OS_FLAGS) (pgrp->OSFlagFlags & flags);	/*将需要检测的事件位提取出来 Extract only the bits we want    */
		if (flags_rdy != (OS_FLAGS) 0) {	/*只要不全是0即可 See if any flag set                      */
			if (consume == OS_TRUE) {	/*是否定义了consume消费标志 See if we need to consume the flags      */
				pgrp->OSFlagFlags &= (OS_FLAGS) ~ flags_rdy;	/*将对应的事件标识位清0 Clear ONLY the flags that we got */
			}
			OSTCBCur->OSTCBFlagsRdy = flags_rdy;	/*将需要检测的事件位保存 Save flags that were ready               */
			OS_EXIT_CRITICAL();	/* Yes, condition met, return to caller     */
			*perr = OS_ERR_NONE;
			return (flags_rdy);
		} else {	/* Block task until events occur or timeout */
			OS_FlagBlock(pgrp, &node, flags, wait_type, timeout);   //如果没有可用的事件标志组，这要等到事件发生或者超时
			OS_EXIT_CRITICAL();
		}
		break;

#if OS_FLAG_WAIT_CLR_EN > 0u
	case OS_FLAG_WAIT_CLR_ALL:	/*要求所有的事件标志位都清零 See if all required flags are cleared    */
		flags_rdy = (OS_FLAGS) ~ pgrp->OSFlagFlags & flags;	/*将需要检测的事件位提取出来，需要先将当前的状态取反 Extract only the bits we want     */
		if (flags_rdy == flags) {	/*检测是否有可用的事件标志组 Must match ALL the bits that we want     */
			if (consume == OS_TRUE) {	/*是否定义了consume消费标志 See if we need to consume the flags      */
				pgrp->OSFlagFlags |= flags_rdy;	/*对指定的事件标志位置位进行恢复 Set ONLY the flags that we wanted        */
			}
			OSTCBCur->OSTCBFlagsRdy = flags_rdy;	/*将需要检测的事件位保存 Save flags that were ready               */
			OS_EXIT_CRITICAL();	/* Yes, condition met, return to caller     */
			*perr = OS_ERR_NONE;
			return (flags_rdy);
		} else {	/* Block task until events occur or timeout */
			OS_FlagBlock(pgrp, &node, flags, wait_type, timeout);   //如果没有可用的事件标志组，这要等到事件发生或者超时
			OS_EXIT_CRITICAL();
		}
		break;

	case OS_FLAG_WAIT_CLR_ANY:                              //任意指定的事件标志位清零
		flags_rdy = (OS_FLAGS) ~ pgrp->OSFlagFlags & flags;	/*提取需要的检测位 Extract only the bits we want      */
		if (flags_rdy != (OS_FLAGS) 0) {	/*检测是否有可用的事件标志组 See if any flag cleared                  */
			if (consume == OS_TRUE) {	/*是否定义了consume标志 See if we need to consume the flags      */
				pgrp->OSFlagFlags |= flags_rdy;	/*对制定的事件标志位置位进行恢复 Set ONLY the flags that we got           */
			}
			OSTCBCur->OSTCBFlagsRdy = flags_rdy;	/* 对需要检测的事件位保存 Save flags that were ready               */
			OS_EXIT_CRITICAL();	/* Yes, condition met, return to caller     */
			*perr = OS_ERR_NONE;
			return (flags_rdy);
		} else {	/* Block task until events occur or timeout */
			OS_FlagBlock(pgrp, &node, flags, wait_type, timeout);  //如果没有可用的事件标志组，这要等到事件发生或者超时
			OS_EXIT_CRITICAL();
		}
		break;
#endif

	default:
		OS_EXIT_CRITICAL();
		flags_rdy = (OS_FLAGS) 0;
		*perr = OS_ERR_FLAG_WAIT_TYPE;          //wait_type不是指定的参数之一
		return (flags_rdy);
	}
/*$PAGE*/
	OS_Sched();		/*调用者被挂起，需要重新调度 Find next HPT ready to run               */
	OS_ENTER_CRITICAL();    //进入临界区，关中断
	//当再次返回时，需要检查是因为超时还是因为事件发生而恢复调用者的运行
	if (OSTCBCur->OSTCBStatPend != OS_STAT_PEND_OK) {	/*如果是因为超时或者是取消挂起而恢复运行 Have we timed-out or aborted?            */
		pend_stat = OSTCBCur->OSTCBStatPend;            //保存当前的任务挂起状态
		OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;      //当前的任务挂起态标志结束
		OS_FlagUnlink(&node);                           //将任务从事件标志等待队列中取下
		OSTCBCur->OSTCBStat = OS_STAT_RDY;	/*任务标志置为就绪态 Yes, make task ready-to-run              */
		OS_EXIT_CRITICAL();
		flags_rdy = (OS_FLAGS) 0;       //没有获取到事件标志，返回0
		switch (pend_stat) {            //查看调用者恢复的具体原因
		case OS_STAT_PEND_ABORT:        //因为取消挂起
			*perr = OS_ERR_PEND_ABORT;	/* Indicate that we aborted   waiting       */
			break;

		case OS_STAT_PEND_TO:           //因为超时
		default:
			*perr = OS_ERR_TIMEOUT;	/* Indicate that we timed-out waiting       */
			break;
		}
		return (flags_rdy);
	}
    //是因为事件发生而恢复了运行
	flags_rdy = OSTCBCur->OSTCBFlagsRdy;    //将需要的事件标志保存
	if (consume == OS_TRUE) {	/*consume消费处理 See if we need to consume the flags      */
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
*              查看任务需要的事件标志组的位
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
	OS_CPU_SR cpu_sr = 0u;      //第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



	OS_ENTER_CRITICAL();
	flags = OSTCBCur->OSTCBFlagsRdy;    //获取需要的事件标志组的位，因为在请求事件标志组过程中，OSTCBCur->OSTCBFlagsRdy = flags_rdy;
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
*              置位或清零事件标志组中的事件标志位
*
* Arguments  : pgrp          is a pointer to the desired event flag group.
*                            指向事件标志组的指针
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
*                            指定需要检验的事件标志位
*
*              opt           indicates whether the flags will be:
*                                set     (OS_FLAG_SET) or
*                                cleared (OS_FLAG_CLR)
*                            指定事件标志位的设置方式，置位or清零
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
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pgrp == (OS_FLAG_GRP *) 0) {	/*确保输入的事件标志组指针可用 Validate 'pgrp'                                */
		*perr = OS_ERR_FLAG_INVALID_PGRP;
		return ((OS_FLAGS) 0);
	}
#endif
	if (pgrp->OSFlagType != OS_EVENT_TYPE_FLAG) {	/*确保事件类型是事件标志组类型 Make sure we are pointing to an event flag grp */
		*perr = OS_ERR_EVENT_TYPE;
		return ((OS_FLAGS) 0);
	}
/*$PAGE*/
	OS_ENTER_CRITICAL();
	switch (opt) {
	case OS_FLAG_CLR:                               //将事件标志组中的指定位清零
		pgrp->OSFlagFlags &= (OS_FLAGS) ~ flags;	/* Clear the flags specified in the group         */
		break;

	case OS_FLAG_SET:                               //将事件标志组中的指定位置1
		pgrp->OSFlagFlags |= flags;	/* Set   the flags specified in the group         */
		break;

	default:                                        //输入参数错误
		OS_EXIT_CRITICAL();	/* INVALID option                                 */
		*perr = OS_ERR_FLAG_INVALID_OPT;
		return ((OS_FLAGS) 0);
	}
	sched = OS_FALSE;	/* 假定对事件标志的操作不会导致更高优先级进入就绪 Indicate that we don't need rescheduling       */
	pnode = (OS_FLAG_NODE *) pgrp->OSFlagWaitList;  //获取等待事件标志的节点
	while (pnode != (OS_FLAG_NODE *) 0) {	/*遍历事件标志组的等待任务列表 Go through all tasks waiting on event flag(s)  */
		switch (pnode->OSFlagNodeWaitType) {//检测每一个等待事件标志的类型
		case OS_FLAG_WAIT_SET_ALL:	/*要求需要的标志全部为1 See if all req. flags are set for current node */
			flags_rdy = (OS_FLAGS) (pgrp->OSFlagFlags & pnode->OSFlagNodeFlags);//提取需要的事件标志位的当前状态
			if (flags_rdy == pnode->OSFlagNodeFlags) {     //当前的事件标志位的状态符合要求
				rdy = OS_FlagTaskRdy(pnode, flags_rdy);	/*将等待事件标志组的任务转入就绪 Make task RTR, event(s) Rx'd          */
				if (rdy == OS_TRUE) {   //转入就绪成功
					sched = OS_TRUE;	/*调度标志置为真 When done we will reschedule          */
				}
			}
			break;          //跳出，检测下一个节点

		case OS_FLAG_WAIT_SET_ANY:	/*要求至少一个为1 See if any flag set                            */
			flags_rdy = (OS_FLAGS) (pgrp->OSFlagFlags & pnode->OSFlagNodeFlags); //提取需要的事件标志位的当前状态
			if (flags_rdy != (OS_FLAGS) 0) {    //当前的事件标志组的状态符合要求，不为零，至少一个为1
				rdy = OS_FlagTaskRdy(pnode, flags_rdy);	/* 将等待事件标志组的任务转入就绪态Make task RTR, event(s) Rx'd          */
				if (rdy == OS_TRUE) {   //转入就绪成功
					sched = OS_TRUE;	/*调度标志置为真 When done we will reschedule          */
				}
			}
			break;          //跳出，检测下一个节点

#if OS_FLAG_WAIT_CLR_EN > 0u
		case OS_FLAG_WAIT_CLR_ALL:	/*要求需要的标志全部为0 See if all req. flags are set for current node */
			flags_rdy = (OS_FLAGS) ~ pgrp->OSFlagFlags & pnode->OSFlagNodeFlags; //提取需要的事件标志位的当前状态，因为要求为0，所以取反
			if (flags_rdy == pnode->OSFlagNodeFlags) {  //当前的事件标志组的状态符合要求
				rdy = OS_FlagTaskRdy(pnode, flags_rdy);	/*将等待事件标志组的任务转入就绪态 Make task RTR, event(s) Rx'd          */
				if (rdy == OS_TRUE) {   //转入就绪成功
					sched = OS_TRUE;	/*调度标志置为真 When done we will reschedule          */
				}
			}
			break;          //跳出，检测下一个节点

		case OS_FLAG_WAIT_CLR_ANY:	/*要求需要的标志至少一个为0 See if any flag set                            */
			flags_rdy = (OS_FLAGS) ~ pgrp->OSFlagFlags & pnode->OSFlagNodeFlags; //提取需要的事件标志位的当前状态，因为要求为0，所以取反
			if (flags_rdy != (OS_FLAGS) 0) {  //当前的事件标志组的状态符合要求
				rdy = OS_FlagTaskRdy(pnode, flags_rdy);	/*将等待事件标志组的任务转入就绪态 Make task RTR, event(s) Rx'd          */
				if (rdy == OS_TRUE) {   //转入就绪成功
					sched = OS_TRUE;	/*调度标志置为真 When done we will reschedule          */
				}
			}
			break;          //跳出，检测下一个节点
#endif
		default:            //输入参数错误
			OS_EXIT_CRITICAL();
			*perr = OS_ERR_FLAG_WAIT_TYPE;
			return ((OS_FLAGS) 0);
		}
		pnode = (OS_FLAG_NODE *) pnode->OSFlagNodeNext;	/*通过链表获得下一个等待节点指针 Point to next task waiting for event flag(s) */
	}
	OS_EXIT_CRITICAL();
	if (sched == OS_TRUE) { //调度标志为真，说明在遍历的过程中有任务转入就绪态
		OS_Sched();
	}
	OS_ENTER_CRITICAL();
	flags_cur = pgrp->OSFlagFlags; //返回当前事件标志位的状态
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
*              查询事件标志组的状态
*
* Arguments  : pgrp         is a pointer to the desired event flag group.
*                           指向事件标志组的指针
*
*              perr          is a pointer to an error code returned to the called:
*                            OS_ERR_NONE                The call was successfull
*                            OS_ERR_FLAG_INVALID_PGRP   You passed a NULL pointer
*                            OS_ERR_EVENT_TYPE          You are not pointing to an event flag group
*
* Returns    : The current value of the event flag group.返回事件标志组的事件标志状态
*
* Called From: Task or ISR
*********************************************************************************************************
*/

#if OS_FLAG_QUERY_EN > 0u
OS_FLAGS OSFlagQuery(OS_FLAG_GRP * pgrp, INT8U * perr)
{
	OS_FLAGS flags;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register          */
	OS_CPU_SR cpu_sr = 0u;      //采用第三种方式开关中断，需要cpu_sr来保存中断状态
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pgrp == (OS_FLAG_GRP *) 0) {	/*确保事件标志组指针可用  Validate 'pgrp'                                   */
		*perr = OS_ERR_FLAG_INVALID_PGRP;
		return ((OS_FLAGS) 0);
	}
#endif
	if (pgrp->OSFlagType != OS_EVENT_TYPE_FLAG) {	/*确保类型正确 Validate event block type                         */
		*perr = OS_ERR_EVENT_TYPE;
		return ((OS_FLAGS) 0);
	}
	OS_ENTER_CRITICAL();
	flags = pgrp->OSFlagFlags;                      //获取当前事件标志状态
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
*              将当前事件挂起等待事件标志或超时
*
* Arguments  : pgrp          is a pointer to the desired event flag group.
*                            请求的事件标志组
*
*              pnode         is a pointer to a structure which contains data about the task waiting for
*                            event flag bit(s) to be set.
*                            为当前任务分配的等待事件标志节点
*
*              flags         Is a bit pattern indicating which bit(s) (i.e. flags) you wish to check.
*                            The bits you want are specified by setting the corresponding bits in
*                            'flags'.  e.g. if your application wants to wait for bits 0 and 1 then
*                            'flags' would contain 0x03.
*                            需要检测的事件标志位
*
*              wait_type     specifies whether you want ALL bits to be set/cleared or ANY of the bits
*                            to be set/cleared.
*                            You can specify the following argument:
*
*                            OS_FLAG_WAIT_CLR_ALL   You will check ALL bits in 'mask' to be clear (0)
*                            OS_FLAG_WAIT_CLR_ANY   You will check ANY bit  in 'mask' to be clear (0)
*                            OS_FLAG_WAIT_SET_ALL   You will check ALL bits in 'mask' to be set   (1)
*                            OS_FLAG_WAIT_SET_ANY   You will check ANY bit  in 'mask' to be set   (1)
*                            检测的类型
*
*              timeout       is the desired amount of time that the task will wait for the event flag
*                            bit(s) to be set.
*                            超时事件
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


	OSTCBCur->OSTCBStat |= OS_STAT_FLAG;            //状态置为等待事件标志组
	OSTCBCur->OSTCBStatPend = OS_STAT_PEND_OK;      //挂起状态
	OSTCBCur->OSTCBDly = timeout;	/*保存等待时间 Store timeout in task's TCB                   */
#if OS_TASK_DEL_EN > 0u
	OSTCBCur->OSTCBFlagNode = pnode;	/*任务TCB指向事件标志节点 TCB to link to node                           */
#endif
	pnode->OSFlagNodeFlags = flags;	/* 保存等待的事件标志位 Save the flags that we need to wait for       */
	pnode->OSFlagNodeWaitType = wait_type;	/*保存等待的类型 Save the type of wait we are doing            */
	pnode->OSFlagNodeTCB = (void *) OSTCBCur;	/*事件标志节点指向任务TCB Link to task's TCB                            */
	pnode->OSFlagNodeNext = pgrp->OSFlagWaitList;	/*加入等待队列 Add node at beginning of event flag wait list */
	pnode->OSFlagNodePrev = (void *) 0;             //设置成为等待队列的队首
	pnode->OSFlagNodeFlagGrp = (void *) pgrp;	/*反向指向事件标志组 Link to Event Flag Group                      */
	pnode_next = (OS_FLAG_NODE *) pgrp->OSFlagWaitList; //提取之前的等待队列的队首节点，即当前的第二个等待事件队列节点
	if (pnode_next != (void *) 0) {	/* 如果是空，表明当前等待队列里只有一个节点，还是刚刚加入的那个 Is this the first NODE to insert?             */
		pnode_next->OSFlagNodePrev = pnode;	/*如果不为空，调整第二个节点的前向指针指向刚刚加入的节点，组成双向链表 No, link in doubly linked list                */
	}
	pgrp->OSFlagWaitList = (void *) pnode;  //事件标志组的等待队列的队首指针调整

	y = OSTCBCur->OSTCBY;	/*挂起当前的任务，从就绪表中去除 Suspend current task until flag(s) received   */
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
*              初始化事件标志组队列
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
#if OS_MAX_FLAGS == 1u                                  //事件标志组最多只有一个
	OSFlagFreeList = (OS_FLAG_GRP *) & OSFlagTbl[0];	/*给事件标志组空闲队列分配空间 Only ONE event flag group!      */
	OSFlagFreeList->OSFlagType = OS_EVENT_TYPE_UNUSED;  //事件标志组状态位未使用
	OSFlagFreeList->OSFlagWaitList = (void *) 0;        //没有等待队列
	OSFlagFreeList->OSFlagFlags = (OS_FLAGS) 0;         //没有事件标志位状态
#if OS_FLAG_NAME_EN > 0u
	OSFlagFreeList->OSFlagName = (INT8U *) "?";         //名字默认
#endif
#endif

#if OS_MAX_FLAGS >= 2u                                  //事件标志组不止一个
	INT16U ix;
	INT16U ix_next;
	OS_FLAG_GRP *pgrp1;
	OS_FLAG_GRP *pgrp2;


	OS_MemClr((INT8U *) & OSFlagTbl[0], sizeof(OSFlagTbl));	/*清理分配给事件标志组空闲队列的空间 Clear the flag group table      */
	for (ix = 0u; ix < (OS_MAX_FLAGS - 1u); ix++) {	/*遍历空闲队列中的每个事件标志组，实际上没有包括最后一个 Init. list of free EVENT FLAGS  */
		ix_next = ix + 1u;                          //下一个的索引
		pgrp1 = &OSFlagTbl[ix];                     //当前的事件标志组
		pgrp2 = &OSFlagTbl[ix_next];                //下一个事件标志组
		pgrp1->OSFlagType = OS_EVENT_TYPE_UNUSED;   //当前的类型为未使用
		pgrp1->OSFlagWaitList = (void *) pgrp2;     //当前的事件标志组的等待队列指向下一个事件标志组，组成单向链表
#if OS_FLAG_NAME_EN > 0u
		pgrp1->OSFlagName = (INT8U *) (void *) "?";	/* 名字默认 Unknown name                    */
#endif
	}
    //对最后一个事件标志组处理
	pgrp1 = &OSFlagTbl[ix];                     //获取最后一个事件标志组的指针
	pgrp1->OSFlagType = OS_EVENT_TYPE_UNUSED;   //类型置为未使用
	pgrp1->OSFlagWaitList = (void *) 0;         //由于是最后一个，后面没有了，所以指针指向0
#if OS_FLAG_NAME_EN > 0u
	pgrp1->OSFlagName = (INT8U *) (void *) "?";	/*名字默认 Unknown name                    */
#endif
	OSFlagFreeList = &OSFlagTbl[0];             //事件标志组的队首指针调整
#endif
}

/*$PAGE*/
/*
*********************************************************************************************************
*                              MAKE TASK READY-TO-RUN, EVENT(s) OCCURRED
*
* Description: This function is internal to uC/OS-II and is used to make a task ready-to-run because the
*              desired event flag bits have been set.
*              由于事件标志发生，将等待的任务转入就绪
*
* Arguments  : pnode         is a pointer to a structure which contains data about the task waiting for
*                            event flag bit(s) to be set.
*                            等待的事件标志节点
*
*              flags_rdy     contains the bit pattern of the event flags that cause the task to become
*                            ready-to-run.
*                            唤醒任务的事件标志位的状态
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


	ptcb = (OS_TCB *) pnode->OSFlagNodeTCB;	/*获取等待的事件标志节点所对应的任务TCB Point to TCB of waiting task             */
	ptcb->OSTCBDly = 0u;                    //延时清0
	ptcb->OSTCBFlagsRdy = flags_rdy;        //将等待的事件标志位重新设置为唤醒任务的事件标志位
	ptcb->OSTCBStat &= (INT8U) ~ (INT8U) OS_STAT_FLAG;//取消事件标志组的等待标志
	ptcb->OSTCBStatPend = OS_STAT_PEND_OK;      //挂起标志结束
	if (ptcb->OSTCBStat == OS_STAT_RDY) {	/*当前任务就绪态了么 Task now ready?                          */
		OSRdyGrp |= ptcb->OSTCBBitY;	/*将任务加入就绪列表 Put task into ready list                 */
		OSRdyTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
		sched = OS_TRUE;                //返回成功标志
	} else {        //任务没有处于就绪态，因为可能不止在等待事件标志组
		sched = OS_FALSE;
	}
	OS_FlagUnlink(pnode);       //从等待列表中摘下
	return (sched);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                  UNLINK EVENT FLAG NODE FROM WAITING LIST
*
* Description: This function is internal to uC/OS-II and is used to unlink an event flag node from a
*              list of tasks waiting for the event flag.
*              从事件标志组的等待链表中摘下事件标志节点
*
* Arguments  : pnode         is a pointer to a structure which contains data about the task waiting for
*                            event flag bit(s) to be set.
*                            事件标志节点指针
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


	pnode_prev = (OS_FLAG_NODE *) pnode->OSFlagNodePrev;          //要摘下节点的前一个节点
	pnode_next = (OS_FLAG_NODE *) pnode->OSFlagNodeNext;          //要摘下节点的下一个节点
	if (pnode_prev == (OS_FLAG_NODE *) 0) {	/*如果前面的节点为空，即要摘下的是队首节点 Is it first node in wait list?      */
		pgrp = (OS_FLAG_GRP *) pnode->OSFlagNodeFlagGrp;    //获取对应的事件标志组
		pgrp->OSFlagWaitList = (void *) pnode_next;	/*事件标志组的等待列表队首指针更新为下一个节点      Update list for new 1st node   */
		if (pnode_next != (OS_FLAG_NODE *) 0) {    //如果下一个节点存在
			pnode_next->OSFlagNodePrev = (OS_FLAG_NODE *) 0;	/*将下一个节点的前向节点指针置为空，因为它现在是队首节点了 Link new 1st node PREV to NULL */
		}
	} else {		/*如果前面的节点不为空，意味着要摘下的不是队首节点 No,  A node somewhere in the list   */
		pnode_prev->OSFlagNodeNext = pnode_next;	/*将前一个节点的后向指针指向下一个节点，跳过当前节点      Link around the node to unlink */
		if (pnode_next != (OS_FLAG_NODE *) 0) {	/* 如果下一个节点存在     Was this the LAST node?        */
			pnode_next->OSFlagNodePrev = pnode_prev;	/* 下一个节点的前向指针改为前一个节点，跳过当前节点   No, Link around current node   */
		}
	}
#if OS_TASK_DEL_EN > 0u
	ptcb = (OS_TCB *) pnode->OSFlagNodeTCB;     //获取节点对应的任务TCB
	ptcb->OSTCBFlagNode = (OS_FLAG_NODE *) 0;   //将指向事件标志节点的指针置为空，意味着节点对应的任务没有需要的事件标志组
#endif
}
#endif
