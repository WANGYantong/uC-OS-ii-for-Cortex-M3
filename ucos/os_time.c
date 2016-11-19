/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*                                             TIME MANAGEMENT
*
*                              (c) Copyright 1992-2009, Micrium, Weston, FL
*                                           All Rights Reserved
*
* File    : OS_TIME.C
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

/*
*********************************************************************************************************
*                                       DELAY TASK 'n' TICKS
*
* Description: This function is called to delay execution of the currently running task until the
*              specified number of system ticks expires.  This, of course, directly equates to delaying
*              the current task for some time to expire.  No delay will result If the specified delay is
*              0.  If the specified delay is greater than 0 then, a context switch will result.
*
* Arguments  : ticks     is the time delay that the task will be suspended in number of clock 'ticks'.
*                        Note that by specifying 0, the task will not be delayed.
*
* Returns    : none
*********************************************************************************************************
*/

void OSTimeDly(INT32U ticks)
{
	INT8U y;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //使用方法三来开关中断，需要cpu_sr来保存中断状态
#endif



	if (OSIntNesting > 0u) {	/* 中断服务程序不能延时 See if trying to call from an ISR                  */
		return;
	}
	if (OSLockNesting > 0u) {	/* 调度锁住了不能延时 See if called with scheduler locked                */
		return;
	}
	if (ticks > 0u) {	/* 参数为0,表示不延时 0 means no delay!                                  */
		OS_ENTER_CRITICAL();
		y = OSTCBCur->OSTCBY;	/* Delay current task                                 */
        //从就绪表中移除当前的任务
		OSRdyTbl[y] &= (OS_PRIO) ~ OSTCBCur->OSTCBBitX;
		if (OSRdyTbl[y] == 0u) {
			OSRdyGrp &= (OS_PRIO) ~ OSTCBCur->OSTCBBitY;
		}
		OSTCBCur->OSTCBDly = ticks;	/*保存延时参数  Load ticks in TCB                */
		OS_EXIT_CRITICAL();
		OS_Sched();	/*切换任务 Find next task to run!                             */
	}
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                     DELAY TASK FOR SPECIFIED TIME
*
* Description: This function is called to delay execution of the currently running task until some time
*              expires.  This call allows you to specify the delay time in HOURS, MINUTES, SECONDS and
*              MILLISECONDS instead of ticks.
*               按照时分秒毫秒进行延时
*
* Arguments  : hours     specifies the number of hours that the task will be delayed (max. is 255)
*              minutes   specifies the number of minutes (max. 59)
*              seconds   specifies the number of seconds (max. 59)
*              ms        specifies the number of milliseconds (max. 999)
*
* Returns    : OS_ERR_NONE
*              OS_ERR_TIME_INVALID_MINUTES
*              OS_ERR_TIME_INVALID_SECONDS
*              OS_ERR_TIME_INVALID_MS
*              OS_ERR_TIME_ZERO_DLY
*              OS_ERR_TIME_DLY_ISR
*
* Note(s)    : The resolution on the milliseconds depends on the tick rate.  For example, you can't do
*              a 10 mS delay if the ticker interrupts every 100 mS.  In this case, the delay would be
*              set to 0.  The actual delay is rounded to the nearest tick.
*********************************************************************************************************
*/

#if OS_TIME_DLY_HMSM_EN > 0u
INT8U OSTimeDlyHMSM(INT8U hours, INT8U minutes, INT8U seconds, INT16U ms)
{
	INT32U ticks;


	if (OSIntNesting > 0u) {	/* See if trying to call from an ISR                  */
		return (OS_ERR_TIME_DLY_ISR);
	}
	if (OSLockNesting > 0u) {	/* See if called with scheduler locked                */
		return (OS_ERR_SCHED_LOCKED);
	}
#if OS_ARG_CHK_EN > 0u
	if (hours == 0u) {
		if (minutes == 0u) {
			if (seconds == 0u) {
				if (ms == 0u) {
					return (OS_ERR_TIME_ZERO_DLY);
				}
			}
		}
	}
	if (minutes > 59u) {
		return (OS_ERR_TIME_INVALID_MINUTES);	/* Validate arguments to be within range              */
	}
	if (seconds > 59u) {
		return (OS_ERR_TIME_INVALID_SECONDS);
	}
	if (ms > 999u) {
		return (OS_ERR_TIME_INVALID_MS);
	}
#endif
	/* Compute the total number of clock ticks required.. */
	/* .. (rounded to the nearest tick)                   */
	ticks = ((INT32U) hours * 3600uL + (INT32U) minutes * 60uL + (INT32U) seconds) * OS_TICKS_PER_SEC
	    + OS_TICKS_PER_SEC * ((INT32U) ms + 500uL / OS_TICKS_PER_SEC) / 1000uL;    //精度为0.5个时钟
	OSTimeDly(ticks);
	return (OS_ERR_NONE);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                         RESUME A DELAYED TASK
*
* Description: This function is used resume a task that has been delayed through a call to either
*              OSTimeDly() or OSTimeDlyHMSM().  Note that you can call this function to resume a
*              task that is waiting for an event with timeout.  This would make the task look
*              like a timeout occurred.
*              用来唤醒被OSTimeDly或者OSTimeDlyHMSM延时的任务，也可以用来唤醒被事件延时的任务
*
* Arguments  : prio                      specifies the priority of the task to resume
*
* Returns    : OS_ERR_NONE               Task has been resumed
*              OS_ERR_PRIO_INVALID       if the priority you specify is higher that the maximum allowed
*                                        (i.e. >= OS_LOWEST_PRIO)
*              OS_ERR_TIME_NOT_DLY       Task is not waiting for time to expire
*              OS_ERR_TASK_NOT_EXIST     The desired task has not been created or has been assigned to a Mutex.
*********************************************************************************************************
*/

#if OS_TIME_DLY_RESUME_EN > 0u
INT8U OSTimeDlyResume(INT8U prio)
{
	OS_TCB *ptcb;
#if OS_CRITICAL_METHOD == 3u	/* Storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //使用第三种方式来开关中断，需要用cpu_sr来保存中断状态
#endif



	if (prio >= OS_LOWEST_PRIO) {
		return (OS_ERR_PRIO_INVALID);
	}
	OS_ENTER_CRITICAL();
	ptcb = OSTCBPrioTbl[prio];	/* Make sure that task exist            */
	if (ptcb == (OS_TCB *) 0) {
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_NOT_EXIST);	/* The task does not exist              */
	}
	if (ptcb == OS_TCB_RESERVED) {
		OS_EXIT_CRITICAL();
		return (OS_ERR_TASK_NOT_EXIST);	/* The task does not exist              */
	}
	if (ptcb->OSTCBDly == 0u) {	/* 确保任务是有延时的 See if task is delayed               */
		OS_EXIT_CRITICAL();
		return (OS_ERR_TIME_NOT_DLY);	/* Indicate that task was not delayed   */
	}

	ptcb->OSTCBDly = 0u;	/*延时清0 Clear the time delay                 */
	if ((ptcb->OSTCBStat & OS_STAT_PEND_ANY) != OS_STAT_RDY) {//如果任务是被事件延时，将其置为超时状态
		ptcb->OSTCBStat &= ~OS_STAT_PEND_ANY;	/* Yes, Clear status flag               */
		ptcb->OSTCBStatPend = OS_STAT_PEND_TO;	/* Indicate PEND timeout                */
	} else {                                    //任务是被OSTimeDly或者OSTimeDlyHMSM延时，将其置为延时结束
		ptcb->OSTCBStatPend = OS_STAT_PEND_OK;
	}
	if ((ptcb->OSTCBStat & OS_STAT_SUSPEND) == OS_STAT_RDY) {	/*任务被挂起? Is task suspended?                   */
		OSRdyGrp |= ptcb->OSTCBBitY;	/*没有，将其置于就绪态 No,  Make ready                      */
		OSRdyTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
		OS_EXIT_CRITICAL();
		OS_Sched();	/*执行任务调度 See if this is new highest priority  */
	} else {
		OS_EXIT_CRITICAL();	/*如果任务被挂起，还需要OSTaskResume() Task may be suspended                */
	}
	return (OS_ERR_NONE);
}
#endif
/*$PAGE*/
/*
*********************************************************************************************************
*                                         GET CURRENT SYSTEM TIME
*
* Description: This function is used by your application to obtain the current value of the 32-bit
*              counter which keeps track of the number of clock ticks.
*
* Arguments  : none
*
* Returns    : The current value of OSTime
*********************************************************************************************************
*/

#if OS_TIME_GET_SET_EN > 0u
INT32U OSTimeGet(void)
{
	INT32U ticks;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;
#endif



	OS_ENTER_CRITICAL();
	ticks = OSTime;
	OS_EXIT_CRITICAL();
	return (ticks);
}
#endif

/*
*********************************************************************************************************
*                                            SET SYSTEM CLOCK
*
* Description: This function sets the 32-bit counter which keeps track of the number of clock ticks.
*
* Arguments  : ticks      specifies the new value that OSTime needs to take.
*
* Returns    : none
*********************************************************************************************************
*/

#if OS_TIME_GET_SET_EN > 0u
void OSTimeSet(INT32U ticks)
{
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;
#endif



	OS_ENTER_CRITICAL();
	OSTime = ticks;
	OS_EXIT_CRITICAL();
}
#endif
