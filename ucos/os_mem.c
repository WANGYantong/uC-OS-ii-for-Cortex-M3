/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*                                            MEMORY MANAGEMENT
*
*                              (c) Copyright 1992-2009, Micrium, Weston, FL
*                                           All Rights Reserved
*
* File    : OS_MEM.C
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

#if (OS_MEM_EN > 0u) && (OS_MAX_MEM_PART > 0u)
/*
*********************************************************************************************************
*                                        CREATE A MEMORY PARTITION
*
* Description : Create a fixed-sized memory partition that will be managed by uC/OS-II.
�*              �����ڴ����
*
* Arguments   : addr     is the starting address of the memory partition
*                        �������ڴ�������ʼ��ַ
*
*               nblks    is the number of memory blocks to create from the partition.
                         �ڴ�������
*
*               blksize  is the size (in bytes) of each block in the memory partition.
*                        ÿ���ڴ��Ĵ�С
*
*               perr     is a pointer to a variable containing an error message which will be set by
*                        this function to either:
*
*                        OS_ERR_NONE              if the memory partition has been created correctly.
*                        OS_ERR_MEM_INVALID_ADDR  if you are specifying an invalid address for the memory
*                                                 storage of the partition or, the block does not align
*                                                 on a pointer boundary
*                        OS_ERR_MEM_INVALID_PART  no free partitions available
*                        OS_ERR_MEM_INVALID_BLKS  user specified an invalid number of blocks (must be >= 2)
*                        OS_ERR_MEM_INVALID_SIZE  user specified an invalid block size
*                                                   - must be greater than the size of a pointer
*                                                   - must be able to hold an integral number of pointers
* Returns    : != (OS_MEM *)0  is the partition was created
*              == (OS_MEM *)0  if the partition was not created because of invalid arguments or, no
*                              free partition is available.
*********************************************************************************************************
*/

OS_MEM *OSMemCreate(void *addr, INT32U nblks, INT32U blksize, INT8U * perr)
{
	OS_MEM *pmem;
	INT8U *pblk;                //������һ��ָ��INT8U���͵�ָ��
	void **plink;               //����һ��ָ��void**���͵�ָ��
	INT32U loops;
	INT32U i;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
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

#if OS_ARG_CHK_EN > 0u
	if (addr == (void *) 0) {	/*ȷ��������ڴ��������ʼ��ַ����Ч�� Must pass a valid address for the memory part. */
		*perr = OS_ERR_MEM_INVALID_ADDR;
		return ((OS_MEM *) 0);
	}
	if (((INT32U) addr & (sizeof(void *) - 1u)) != 0u) {	/*��֤��ַ��4�ֽڶ���ġ�sizeof(void*)=4�ֽڣ���1Ϊ3����������λΪ0 Must be pointer size aligned                */
		*perr = OS_ERR_MEM_INVALID_ADDR;
		return ((OS_MEM *) 0);
	}
	if (nblks < 2u) {	/*ÿ���ڴ���������������ڴ�� Must have at least 2 blocks per partition     */
		*perr = OS_ERR_MEM_INVALID_BLKS;
		return ((OS_MEM *) 0);
	}
	if (blksize < sizeof(void *)) {	/*ÿ���ڴ��������������һ��ָ�� Must contain space for at least a pointer     */
		*perr = OS_ERR_MEM_INVALID_SIZE;
		return ((OS_MEM *) 0);
	}
#endif
	OS_ENTER_CRITICAL();
	pmem = OSMemFreeList;	/*�ӿ����ڴ���ƿ�������ȡ��һ���ڴ���ƿ� Get next free memory partition                */
	if (OSMemFreeList != (OS_MEM *) 0) {	/*ȷ��ȡ�����ڴ���ƿ���� See if pool of free partitions was empty      */
		OSMemFreeList = (OS_MEM *) OSMemFreeList->OSMemFreeList;    //���������ڴ���ƿ�����ָ��
	}
	OS_EXIT_CRITICAL();
	if (pmem == (OS_MEM *) 0) {	/*ȷ���ڴ���ƿ���� See if we have a memory partition             */
		*perr = OS_ERR_MEM_INVALID_PART;
		return ((OS_MEM *) 0);
	}
    /****************************************************************************************************************/
    //����Ҫ�������ڴ�����ڵ������ڴ�����ӳ�һ����������

    //��addrָ���ֵ��Ҳ�����ڴ������ʼ��ַǿ��ת����ָ��void * ���͵�һ��ָ�룬��ֵ��plink��������Ϊ�˷�ֹ������������ֵ����һ���ģ�
	plink = (void **) addr;	/* Create linked list of free memory blocks      */
    //��addrָ���ֵǿ��ת����ָ��INT8U���͵�һ��ָ�룬��ֵ��pblk����ʱaddr��pblk��plink�ж��洢���ڴ������ʼ��ַ
	pblk = (INT8U *) addr;
    //����ѭ���������ڴ������һ����Ϊ��һ�鲻�÷�����
	loops = nblks - 1u;
	for (i = 0u; i < loops; i++) {
        //pblk+blksize�õ��µĵ�ַ����ֵ��pblk��
        //ָ�����������INT8U����ռһ���ڴ��ַ����INT32U����ռ4���ڴ��ַ������ָ���������ֵΪָ���ַ+��INT8Uռ�ڴ��ַ������blksize
        //pblkָ������һ�ڴ�����ʼ��ַ��
		pblk += blksize;	/* Point to the FOLLOWING block                  */
        //��pblkָ���ֵǿ��ת����һ��ָ��void���͵�ָ�룬��ֵ��plink��ָ��ַ�ڴ��ֵ�����ø��ڴ��������void * ��
        //�����Ǹ��ڴ����׵�ַ���ڴ��ֵΪ��һ�ڴ�����ʼ��ַ
		*plink = (void *) pblk;	/* Save pointer to NEXT block in CURRENT block   */
        //��pblkָ���ֵǿ��ת����һ��ָ��void*���͵�ָ�룬��ֵ��plink
        //�´�ʹ��*plinkʱ�Ѿ�����һ�ڴ����ʼ��ַ���ڴ��ֵ�ˣ���Ϊplink��ֵ����һ�ڴ�����ʼ��ַ��
		plink = (void **) pblk;	/* Position to  NEXT      block                  */
	}

	*plink = (void *) 0;	/*���һ���ڴ���ָ��ָ��NULL Last memory block points to NULL              */
    /****************************************************************************************************************/
	pmem->OSMemAddr = addr;	/*���ڴ���ƿ��б����ڴ������ʼ��ַ Store start address of memory partition       */
	pmem->OSMemFreeList = addr;	/*ָ���ڴ�����ڵ�һ�����е��ڴ�� Initialize pointer to pool of free blocks     */
	pmem->OSMemNFree = nblks;	/*��������ڴ������ Store number of free blocks in MCB            */
	pmem->OSMemNBlks = nblks;   //�����ܵ��ڴ�������
	pmem->OSMemBlkSize = blksize;	/*�����ڴ��Ĵ�С Store block size of each memory blocks        */
	*perr = OS_ERR_NONE;
	return (pmem);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                          GET A MEMORY BLOCK
*
* Description : Get a memory block from a partition
*               ���Ѿ��������ڴ����������һ���ڴ��
*
* Arguments   : pmem    is a pointer to the memory partition control block
*                       ָ���ڴ���ƿ��ָ��
*
*               perr    is a pointer to a variable containing an error message which will be set by this
*                       function to either:
*
*                       OS_ERR_NONE             if the memory partition has been created correctly.
*                       OS_ERR_MEM_NO_FREE_BLKS if there are no more free memory blocks to allocate to caller
*                       OS_ERR_MEM_INVALID_PMEM if you passed a NULL pointer for 'pmem'
*
* Returns     : A pointer to a memory block if no error is detected
*               A pointer to NULL if an error is detected
*********************************************************************************************************
*/

void *OSMemGet(OS_MEM * pmem, INT8U * perr)
{
	void *pblk;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ�������Ҫcpu_sr�������ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pmem == (OS_MEM *) 0) {	/*ȷ��ָ��ָ����ڴ���ƿ��ǿ��õ� Must point to a valid memory partition        */
		*perr = OS_ERR_MEM_INVALID_PMEM;
		return ((void *) 0);
	}
#endif
	OS_ENTER_CRITICAL();
	if (pmem->OSMemNFree > 0u) {	/*���е��ڴ����Ŀ������ See if there are any free memory blocks       */
		pblk = pmem->OSMemFreeList;	/*����һ�������ڴ��ӿ����ڴ��������ɾ�� Yes, point to next free memory block          */
		pmem->OSMemFreeList = *(void **) pblk;	/*���������ڴ��ָ��      Adjust pointer to new free list          */
		pmem->OSMemNFree--;	/*�����ڴ��������1      One less memory block in this partition  */
		OS_EXIT_CRITICAL();
		*perr = OS_ERR_NONE;	/*      No error                                 */
		return (pblk);	/*      Return memory block to caller            */
	}
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_MEM_NO_FREE_BLKS;	/* No,  Notify caller of empty memory partition  */
	return ((void *) 0);	/*      Return NULL pointer to caller            */
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                   GET THE NAME OF A MEMORY PARTITION
*
* Description: This function is used to obtain the name assigned to a memory partition.
*              ��ȡ�ڴ���ƿ������
*
* Arguments  : pmem      is a pointer to the memory partition
*                        �ڴ���ƿ�����
*
*              pname     is a pointer to a pointer to an ASCII string that will receive the name of the memory partition.
*                        �������ֵ�ָ��
*
*              perr      is a pointer to an error code that can contain one of the following values:
*
*                        OS_ERR_NONE                if the name was copied to 'pname'
*                        OS_ERR_MEM_INVALID_PMEM    if you passed a NULL pointer for 'pmem'
*                        OS_ERR_PNAME_NULL          You passed a NULL pointer for 'pname'
*                        OS_ERR_NAME_GET_ISR        You called this function from an ISR
*
* Returns    : The length of the string or 0 if 'pmem' is a NULL pointer.
*********************************************************************************************************
*/

#if OS_MEM_NAME_EN > 0u
INT8U OSMemNameGet(OS_MEM * pmem, INT8U ** pname, INT8U * perr)
{
	INT8U len;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //�����ַ�ʽ�����жϣ���Ҫcpu_sr�����ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pmem == (OS_MEM *) 0) {	/*ȷ���ڴ���ƿ���� Is 'pmem' a NULL pointer?                          */
		*perr = OS_ERR_MEM_INVALID_PMEM;
		return (0u);
	}
	if (pname == (INT8U **) 0) {	/*ȷ���������ֵ�ָ����� Is 'pname' a NULL pointer?                         */
		*perr = OS_ERR_PNAME_NULL;
		return (0u);
	}
#endif
	if (OSIntNesting > 0u) {	/*ȷ���������жϷ������������ See if trying to call from an ISR                  */
		*perr = OS_ERR_NAME_GET_ISR;
		return (0u);
	}
	OS_ENTER_CRITICAL();
	*pname = pmem->OSMemName;   //��ָ��������ָ�뱣�浽pnameָ��ĵ�ַ��
	len = OS_StrLen(*pname);    //���������ĳ���
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
	return (len);
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                 ASSIGN A NAME TO A MEMORY PARTITION
*
* Description: This function assigns a name to a memory partition.
*              �����ڴ���ƿ�����
*
* Arguments  : pmem      is a pointer to the memory partition
*                        �ڴ���ƿ�ָ��
*
*              pname     is a pointer to an ASCII string that contains the name of the memory partition.
*                        ָ�����ֵ�ָ��
*
*              perr      is a pointer to an error code that can contain one of the following values:
*
*                        OS_ERR_NONE                if the name was copied to 'pname'
*                        OS_ERR_MEM_INVALID_PMEM    if you passed a NULL pointer for 'pmem'
*                        OS_ERR_PNAME_NULL          You passed a NULL pointer for 'pname'
*                        OS_ERR_MEM_NAME_TOO_LONG   if the name doesn't fit in the storage area
*                        OS_ERR_NAME_SET_ISR        if you called this function from an ISR
*
* Returns    : None
*********************************************************************************************************
*/

#if OS_MEM_NAME_EN > 0u
void OSMemNameSet(OS_MEM * pmem, INT8U * pname, INT8U * perr)
{
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //�����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pmem == (OS_MEM *) 0) {	/*ȷ���ڴ���ƿ�ָ����� Is 'pmem' a NULL pointer?                          */
		*perr = OS_ERR_MEM_INVALID_PMEM;
		return;
	}
	if (pname == (INT8U *) 0) {	/*ȷ��������Ч Is 'pname' a NULL pointer?                         */
		*perr = OS_ERR_PNAME_NULL;
		return;
	}
#endif
	if (OSIntNesting > 0u) {	/*ȷ���������жϷ������е��� See if trying to call from an ISR                  */
		*perr = OS_ERR_NAME_SET_ISR;
		return;
	}
	OS_ENTER_CRITICAL();
	pmem->OSMemName = pname;   //����
	OS_EXIT_CRITICAL();
	*perr = OS_ERR_NONE;
}
#endif

/*$PAGE*/
/*
*********************************************************************************************************
*                                         RELEASE A MEMORY BLOCK
*
* Description : Returns a memory block to a partition
*               �ͷ��ڴ��
*
* Arguments   : pmem    is a pointer to the memory partition control block
*                       ָ���ڴ���ƿ��ָ��
*
*               pblk    is a pointer to the memory block being released.
*                       ָ��Ҫ���ͷŵ��ڴ���ָ��
*
* Returns     : OS_ERR_NONE              if the memory block was inserted into the partition
*               OS_ERR_MEM_FULL          if you are returning a memory block to an already FULL memory
*                                        partition (You freed more blocks than you allocated!)
*               OS_ERR_MEM_INVALID_PMEM  if you passed a NULL pointer for 'pmem'
*               OS_ERR_MEM_INVALID_PBLK  if you passed a NULL pointer for the block to release.
*********************************************************************************************************
*/

INT8U OSMemPut(OS_MEM * pmem, void *pblk)
{
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�����ж�״̬
#endif



#if OS_ARG_CHK_EN > 0u
	if (pmem == (OS_MEM *) 0) {	/*ȷ��ָ��ָ����ڴ������Ч�� Must point to a valid memory partition             */
		return (OS_ERR_MEM_INVALID_PMEM);
	}
	if (pblk == (void *) 0) {	/*ȷ���ͷŵ��ڴ������Ч�� Must release a valid block                         */
		return (OS_ERR_MEM_INVALID_PBLK);
	}
#endif
	OS_ENTER_CRITICAL();
	if (pmem->OSMemNFree >= pmem->OSMemNBlks) {	/*����ڴ�����Ƿ����� Make sure all blocks not already returned          */
		OS_EXIT_CRITICAL();
		return (OS_ERR_MEM_FULL);
	}
	*(void **) pblk = pmem->OSMemFreeList;	/*���û������ǰ�ڴ��ָ����ж��еĵ�һ���ڴ�� Insert released block into free block list         */
	pmem->OSMemFreeList = pblk;             //����ָ�룬���ͷŵ��ڴ����ڿ�������Ķ���
	pmem->OSMemNFree++;	/*����ֵ���� One more memory block in this partition            */
	OS_EXIT_CRITICAL();
	return (OS_ERR_NONE);	/* Notify caller that memory block was released       */
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                          QUERY MEMORY PARTITION
*
* Description : This function is used to determine the number of free memory blocks and the number of
*               used memory blocks from a memory partition.
*               ��ѯ�ڴ������״̬
*
* Arguments   : pmem        is a pointer to the memory partition control block
*                           ָ���ڴ������ָ��
*
*               p_mem_data  is a pointer to a structure that will contain information about the memory
*                           partition.
*                           �����ѯ���
*
* Returns     : OS_ERR_NONE               if no errors were found.
*               OS_ERR_MEM_INVALID_PMEM   if you passed a NULL pointer for 'pmem'
*               OS_ERR_MEM_INVALID_PDATA  if you passed a NULL pointer to the data recipient.
*********************************************************************************************************
*/

#if OS_MEM_QUERY_EN > 0u
INT8U OSMemQuery(OS_MEM * pmem, OS_MEM_DATA * p_mem_data)
{
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register           */
	OS_CPU_SR cpu_sr = 0u;      //���õ����ַ�ʽ�����жϣ���Ҫcpu_sr�������ж�״̬
#endif



#if OS_ARG_CHK_EN > 0u
	if (pmem == (OS_MEM *) 0) {	/*ȷ��ָ��ָ����ڴ���ƿ�����Ч�� Must point to a valid memory partition             */
		return (OS_ERR_MEM_INVALID_PMEM);
	}
	if (p_mem_data == (OS_MEM_DATA *) 0) {	/*ȷ�������ѯ��������ݽṹ���� Must release a valid storage area for the data     */
		return (OS_ERR_MEM_INVALID_PDATA);
	}
#endif
	OS_ENTER_CRITICAL();                            //��ֹ�жϽ�����޸�ĳЩ��������Ҫ���ж�
	p_mem_data->OSAddr = pmem->OSMemAddr;           //��ѯ�ڴ������ʼ��ַ
	p_mem_data->OSFreeList = pmem->OSMemFreeList;   //��ѯ�����ڴ�����ʼ��ַ
	p_mem_data->OSBlkSize = pmem->OSMemBlkSize;     //��ѯÿ���ڴ��Ĵ�С
	p_mem_data->OSNBlks = pmem->OSMemNBlks;         //��ѯ�ڴ�������ڴ������
	p_mem_data->OSNFree = pmem->OSMemNFree;         //��ѯ�ڴ�����п��е��ڴ����Ŀ
	OS_EXIT_CRITICAL();
	p_mem_data->OSNUsed = p_mem_data->OSNBlks - p_mem_data->OSNFree;    //�����Ѿ�ʹ�õ��ڴ����Ŀ
	return (OS_ERR_NONE);
}
#endif				/* OS_MEM_QUERY_EN                                    */
/*$PAGE*/
/*
*********************************************************************************************************
*                                    INITIALIZE MEMORY PARTITION MANAGER
*
* Description : This function is called by uC/OS-II to initialize the memory partition manager.  Your
*               application MUST NOT call this function.
*               ��ʼ���ڴ���ƿ�
*
* Arguments   : none
*
* Returns     : none
*
* Note(s)    : This function is INTERNAL to uC/OS-II and your application should not call it.
*********************************************************************************************************
*/

void OS_MemInit(void)
{
#if OS_MAX_MEM_PART == 1u                                   //���ֻ��һ���ڴ���ƿ�
	OS_MemClr((INT8U *) & OSMemTbl[0], sizeof(OSMemTbl));	/*����ռ� Clear the memory partition table          */
	OSMemFreeList = (OS_MEM *) & OSMemTbl[0];	/*�����ڴ����������ָ��ָ����Ψһ��һ���ڴ���ƿ� Point to beginning of free list           */
#if OS_MEM_NAME_EN > 0u
	OSMemFreeList->OSMemName = (INT8U *) "?";	/*����Ĭ�� Unknown name                              */
#endif
#endif

#if OS_MAX_MEM_PART >= 2u                                   //��ֹһ���ڴ���ƿ�
	OS_MEM *pmem;
	INT16U i;


	OS_MemClr((INT8U *) & OSMemTbl[0], sizeof(OSMemTbl));	/*���������ڴ���ƿ�����Ŀռ� Clear the memory partition table          */
	for (i = 0u; i < (OS_MAX_MEM_PART - 1u); i++) {	/*���������е������ڴ���ƿ飬�������һ�� Init. list of free memory partitions      */
		pmem = &OSMemTbl[i];	/*��ȡ��ǰ���ڴ���ƿ��ַ Point to memory control block (MCB)       */
		pmem->OSMemFreeList = (void *) &OSMemTbl[i + 1u];	/*��ǰ���ڴ���ƿ�ָ����һ���ڴ���ƿ� Chain list of free partitions             */
#if OS_MEM_NAME_EN > 0u
		pmem->OSMemName = (INT8U *) (void *) "?";           //����Ĭ��
#endif
	}
	pmem = &OSMemTbl[i];                //���һ���ڴ���ƿ�
	pmem->OSMemFreeList = (void *) 0;	/*���һ��ָ��NULL Initialize last node                      */
#if OS_MEM_NAME_EN > 0u
	pmem->OSMemName = (INT8U *) (void *) "?";   //����Ĭ��
#endif

	OSMemFreeList = &OSMemTbl[0];	/*��������ָ��ָ���һ���ڴ���ƿ� Point to beginning of free list           */
#endif
}
#endif				/* OS_MEM_EN                                 */
