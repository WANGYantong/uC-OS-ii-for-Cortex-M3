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
* If you plan on using  uC/OS-II  in a commercial product you need to contact Micriµm to properly license
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
å*              ½¨Á¢ÄÚ´æ·ÖÇø
*
* Arguments   : addr     is the starting address of the memory partition
*                        ½¨Á¢µÄÄÚ´æÇøµÄÆğÊ¼µØÖ·
*
*               nblks    is the number of memory blocks to create from the partition.
                         ÄÚ´æ¿éµÄÊıÁ¿
*
*               blksize  is the size (in bytes) of each block in the memory partition.
*                        Ã¿¸öÄÚ´æ¿éµÄ´óĞ¡
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
	INT8U *pblk;                //¶¨ÒåÁËÒ»¸öÖ¸ÏòINT8UÀàĞÍµÄÖ¸Õë
	void **plink;               //¶¨ÒåÒ»¸öÖ¸Ïòvoid**ÀàĞÍµÄÖ¸Õë
	INT32U loops;
	INT32U i;
#if OS_CRITICAL_METHOD == 3u	/* Allocate storage for CPU status register      */
	OS_CPU_SR cpu_sr = 0u;      //²ÉÓÃµÚÈıÖÖ·½Ê½¿ª¹ØÖĞ¶Ï£¬ĞèÒªcpu_srÀ´±£´æÖĞ¶Ï×´Ì¬
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
	if (addr == (void *) 0) {	/*È·±£¶¨ÒåµÄÄÚ´æ·ÖÇøµÄÆğÊ¼µØÖ·ÊÇÓĞĞ§µÄ Must pass a valid address for the memory part. */
		*perr = OS_ERR_MEM_INVALID_ADDR;
		return ((OS_MEM *) 0);
	}
	if (((INT32U) addr & (sizeof(void *) - 1u)) != 0u) {	/*±£Ö¤µØÖ·ÊÇ4×Ö½Ú¶ÔÆëµÄ¡£sizeof(void*)=4×Ö½Ú£¬¼õ1Îª3£¬¼ì²â×îµÍÁ½Î»Îª0 Must be pointer size aligned                */
		*perr = OS_ERR_MEM_INVALID_ADDR;
		return ((OS_MEM *) 0);
	}
	if (nblks < 2u) {	/*Ã¿¸öÄÚ´æ·ÖÇøÖÁÉÙÓĞÁ½¸öÄÚ´æ¿é Must have at least 2 blocks per partition     */
		*perr = OS_ERR_MEM_INVALID_BLKS;
		return ((OS_MEM *) 0);
	}
	if (blksize < sizeof(void *)) {	/*Ã¿¸öÄÚ´æ¿éÖÁÉÙÄÜÈİÄÉÏÂÒ»¸öÖ¸Õë Must contain space for at least a pointer     */
		*perr = OS_ERR_MEM_INVALID_SIZE;
		return ((OS_MEM *) 0);
	}
#endif
	OS_ENTER_CRITICAL();
	pmem = OSMemFreeList;	/*´Ó¿ÕÏĞÄÚ´æ¿ØÖÆ¿éÁ´±íÖĞÈ¡³öÒ»¸öÄÚ´æ¿ØÖÆ¿é Get next free memory partition                */
	if (OSMemFreeList != (OS_MEM *) 0) {	/*È·±£È¡³öµÄÄÚ´æ¿ØÖÆ¿é¿ÉÓÃ See if pool of free partitions was empty      */
		OSMemFreeList = (OS_MEM *) OSMemFreeList->OSMemFreeList;    //µ÷Õû¿ÕÏĞÄÚ´æ¿ØÖÆ¿éÁ´±íÖ¸Õë
	}
	OS_EXIT_CRITICAL();
	if (pmem == (OS_MEM *) 0) {	/*È·±£ÄÚ´æ¿ØÖÆ¿é¿ÉÓÃ See if we have a memory partition             */
		*perr = OS_ERR_MEM_INVALID_PART;
		return ((OS_MEM *) 0);
	}
    /****************************************************************************************************************/
    //½«ËùÒª½¨Á¢µÄÄÚ´æ·ÖÇøÄÚµÄËùÓĞÄÚ´æ¿éÁ´½Ó³ÉÒ»¸öµ¥ÏòÁ´±í

    //½«addrÖ¸ÕëµÄÖµ£¬Ò²¾ÍÊÇÄÚ´æ·ÖÇøÆğÊ¼µØÖ·Ç¿ÖÆ×ª»»³ÉÖ¸Ïòvoid * ÀàĞÍµÄÒ»¸öÖ¸Õë£¬¸³Öµ¸øplink¡££¨ÕâÊÇÎªÁË·ÀÖ¹±àÒëÆ÷³ö´í£¬ÊıÖµ¶¼ÊÇÒ»ÑùµÄ£©
	plink = (void **) addr;	/* Create linked list of free memory blocks      */
    //½«addrÖ¸ÕëµÄÖµÇ¿ÖÆ×ª»»³ÉÖ¸ÏòINT8UÀàĞÍµÄÒ»¸öÖ¸Õë£¬¸³Öµ¸øpblk¡£´ËÊ±addr¡¢pblk¡¢plinkÖĞ¶¼´æ´¢ÁËÄÚ´æ·ÖÇøÆğÊ¼µØÖ·
	pblk = (INT8U *) addr;
    //¼ÆËãÑ­»·´ÎÊı£¬ÄÚ´æ¿éÊı¼õÒ»£¬ÒòÎªµÚÒ»¿é²»ÓÃ·ÖÅäÁË
	loops = nblks - 1u;
	for (i = 0u; i < loops; i++) {
        //pblk+blksizeµÃµ½ĞÂµÄµØÖ·£¬¸³Öµ¸øpblk¡£
        //Ö¸Õë¼ÓÕûÊı£ºÈçINT8UÀàĞÍÕ¼Ò»ÌõÄÚ´æµØÖ·£¬¶øINT32UÀàĞÍÕ¼4ÌõÄÚ´æµØÖ·£¬ËùÒÔÖ¸Õë¼ÓÕûÊıµÄÖµÎªÖ¸ÕëµØÖ·+£¨INT8UÕ¼ÄÚ´æµØÖ·Êı£©¡Áblksize
        //pblkÖ¸Õë±ä³ÉÏÂÒ»ÄÚ´æ¿éµÄÆğÊ¼µØÖ·¡£
		pblk += blksize;	/* Point to the FOLLOWING block                  */
        //½«pblkÖ¸ÕëµÄÖµÇ¿ÖÆ×ª»»³ÉÒ»¸öÖ¸ÏòvoidÀàĞÍµÄÖ¸Õë£¬¸³Öµ¸øplinkËùÖ¸µØÖ·ÄÚ´æµÄÖµ£¬ÕıºÃ¸ÃÄÚ´æµÄÀàĞÍÊÇvoid * ¡£
        //ÒâÒåÊÇ¸ÃÄÚ´æ¿éµÄÊ×µØÖ·µÄÄÚ´æµÄÖµÎªÏÂÒ»ÄÚ´æ¿éµÄÆğÊ¼µØÖ·
		*plink = (void *) pblk;	/* Save pointer to NEXT block in CURRENT block   */
        //½«pblkÖ¸ÕëµÄÖµÇ¿ÖÆ×ª»»³ÉÒ»¸öÖ¸Ïòvoid*ÀàĞÍµÄÖ¸Õë£¬¸³Öµ¸øplink
        //ÏÂ´ÎÊ¹ÓÃ*plinkÊ±ÒÑ¾­ÊÇÏÂÒ»ÄÚ´æ¿éÆğÊ¼µØÖ·µÄÄÚ´æµÄÖµÁË£¬ÒòÎªplinkµÄÖµÊÇÏÂÒ»ÄÚ´æ¿éµÄÆğÊ¼µØÖ·¡£
		plink = (void **) pblk;	/* Position to  NEXT      block                  */
	}

	*plink = (void *) 0;	/*×îºóÒ»¿éÄÚ´æ¿éµÄÖ¸ÕëÖ¸ÏòNULL Last memory block points to NULL              */
    /****************************************************************************************************************/
	pmem->OSMemAddr = addr;	/*ÔÚÄÚ´æ¿ØÖÆ¿éÖĞ±£´æÄÚ´æ·ÖÇøÆğÊ¼µØÖ· Store start address of memory partition       */
	pmem->OSMemFreeList = addr;	/*Ö¸ÏòÄÚ´æ·ÖÇøÄÚµÚÒ»¸ö¿ÕÏĞµÄÄÚ´æ¿é Initialize pointer to pool of free blocks     */
	pmem->OSMemNFree = nblks;	/*±£´æ¿ÕÏĞÄÚ´æ¿éÊıÁ¿ Store number of free blocks in MCB            */
	pmem->OSMemNBlks = nblks;   //±£´æ×ÜµÄÄÚ´æ¿éµÄÊıÁ¿
	pmem->OSMemBlkSize = blksize;	/*±£´æÄÚ´æ¿éµÄ´óĞ¡ Store block size of each memory blocks        */
	*perr = OS_ERR_NONE;
	return (pmem);
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                          GET A MEMORY BLOCK
*
* Description : Get a memory block from a partition
*               ´ÓÒÑ¾­½¨Á¢µÄÄÚ´æ·ÖÇøÖĞÉêÇëÒ»¸öÄÚ´æ¿é
*
* Arguments   : pmem    is a pointer to the memory partition control block
*                       Ö¸ÏòÄÚ´æ¿ØÖÆ¿éµÄÖ¸Õë
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
	OS_CPU_SR cpu_sr = 0u;      //²ÉÓÃµÚÈıÖÖ·½Ê½¿ª¹ØÖĞ¶Ï£¬ËùÒÔĞèÒªcpu_srÀ´±£´æÖĞ¶Ï×´Ì¬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pmem == (OS_MEM *) 0) {	/*È·±£Ö¸ÕëÖ¸ÏòµÄÄÚ´æ¿ØÖÆ¿éÊÇ¿ÉÓÃµÄ Must point to a valid memory partition        */
		*perr = OS_ERR_MEM_INVALID_PMEM;
		return ((void *) 0);
	}
#endif
	OS_ENTER_CRITICAL();
	if (pmem->OSMemNFree > 0u) {	/*¿ÕÏĞµÄÄÚ´æ¿éÊıÄ¿´óÓÚÁã See if there are any free memory blocks       */
		pblk = pmem->OSMemFreeList;	/*½«µÚÒ»¸ö¿ÕÏĞÄÚ´æ¿é´Ó¿ÕÏĞÄÚ´æ¿éÁ´±íÖĞÉ¾³ı Yes, point to next free memory block          */
		pmem->OSMemFreeList = *(void **) pblk;	/*µ÷Õû¿ÕÏĞÄÚ´æ¿éÖ¸Õë      Adjust pointer to new free list          */
		pmem->OSMemNFree--;	/*¿ÕÏĞÄÚ´æ¿éÊıÁ¿¼õ1      One less memory block in this partition  */
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
*              »ñÈ¡ÄÚ´æ¿ØÖÆ¿éµÄÃû×Ö
*
* Arguments  : pmem      is a pointer to the memory partition
*                        ÄÚ´æ¿ØÖÆ¿éÃû×Ö
*
*              pname     is a pointer to a pointer to an ASCII string that will receive the name of the memory partition.
*                        ±£´æÃû×ÖµÄÖ¸Õë
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
	OS_CPU_SR cpu_sr = 0u;      //µÚÈıÖÖ·½Ê½¿ª¹ØÖĞ¶Ï£¬ĞèÒªcpu_sr±£´æÖĞ¶Ï×´Ì¬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pmem == (OS_MEM *) 0) {	/*È·±£ÄÚ´æ¿ØÖÆ¿é¿ÉÓÃ Is 'pmem' a NULL pointer?                          */
		*perr = OS_ERR_MEM_INVALID_PMEM;
		return (0u);
	}
	if (pname == (INT8U **) 0) {	/*È·±£±£´æÃû×ÖµÄÖ¸Õë¿ÉÓÃ Is 'pname' a NULL pointer?                         */
		*perr = OS_ERR_PNAME_NULL;
		return (0u);
	}
#endif
	if (OSIntNesting > 0u) {	/*È·±£²»ÄÜÔÚÖĞ¶Ï·şÎñº¯ÊıÀïÃæµ÷ÓÃ See if trying to call from an ISR                  */
		*perr = OS_ERR_NAME_GET_ISR;
		return (0u);
	}
	OS_ENTER_CRITICAL();
	*pname = pmem->OSMemName;   //½«Ö¸ÏòĞÕÃûµÄÖ¸Õë±£´æµ½pnameÖ¸ÏòµÄµØÖ·ÖĞ
	len = OS_StrLen(*pname);    //·µ»ØĞÕÃûµÄ³¤¶È
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
*              ÉèÖÃÄÚ´æ¿ØÖÆ¿éÃû³Æ
*
* Arguments  : pmem      is a pointer to the memory partition
*                        ÄÚ´æ¿ØÖÆ¿éÖ¸Õë
*
*              pname     is a pointer to an ASCII string that contains the name of the memory partition.
*                        Ö¸ÏòÃû×ÖµÄÖ¸Õë
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
	OS_CPU_SR cpu_sr = 0u;      //µÚÈıÖÖ·½Ê½¿ª¹ØÖĞ¶Ï£¬ĞèÒªcpu_srÀ´±£´æÖĞ¶Ï×´Ì¬
#endif



#ifdef OS_SAFETY_CRITICAL
	if (perr == (INT8U *) 0) {
		OS_SAFETY_CRITICAL_EXCEPTION();
	}
#endif

#if OS_ARG_CHK_EN > 0u
	if (pmem == (OS_MEM *) 0) {	/*È·±£ÄÚ´æ¿ØÖÆ¿éÖ¸Õë¿ÉÓÃ Is 'pmem' a NULL pointer?                          */
		*perr = OS_ERR_MEM_INVALID_PMEM;
		return;
	}
	if (pname == (INT8U *) 0) {	/*È·±£Ãû×ÖÓĞĞ§ Is 'pname' a NULL pointer?                         */
		*perr = OS_ERR_PNAME_NULL;
		return;
	}
#endif
	if (OSIntNesting > 0u) {	/*È·±£²»ÊÇÔÚÖĞ¶Ï·şÎñº¯ÊıÖĞµ÷ÓÃ See if trying to call from an ISR                  */
		*perr = OS_ERR_NAME_SET_ISR;
		return;
	}
	OS_ENTER_CRITICAL();
	pmem->OSMemName = pname;   //ÆğÃû
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
*               ÊÍ·ÅÄÚ´æ¿é
*
* Arguments   : pmem    is a pointer to the memory partition control block
*                       Ö¸ÏòÄÚ´æ¿ØÖÆ¿éµÄÖ¸Õë
*
*               pblk    is a pointer to the memory block being released.
*                       Ö¸Ïò½«Òª±»ÊÍ·ÅµÄÄÚ´æ¿éµÄÖ¸Õë
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
	OS_CPU_SR cpu_sr = 0u;      //²ÉÓÃµÚÈıÖÖ·½Ê½¿ª¹ØÖĞ¶Ï£¬ĞèÒªcpu_sr±£´æÖĞ¶Ï×´Ì¬
#endif



#if OS_ARG_CHK_EN > 0u
	if (pmem == (OS_MEM *) 0) {	/*È·±£Ö¸ÕëÖ¸ÏòµÄÄÚ´æ¿éÊÇÓĞĞ§µÄ Must point to a valid memory partition             */
		return (OS_ERR_MEM_INVALID_PMEM);
	}
	if (pblk == (void *) 0) {	/*È·±£ÊÍ·ÅµÄÄÚ´æ¿éÊÇÓĞĞ§µÄ Must release a valid block                         */
		return (OS_ERR_MEM_INVALID_PBLK);
	}
#endif
	OS_ENTER_CRITICAL();
	if (pmem->OSMemNFree >= pmem->OSMemNBlks) {	/*¼ì²éÄÚ´æ·ÖÇøÊÇ·ñÒÑÂú Make sure all blocks not already returned          */
		OS_EXIT_CRITICAL();
		return (OS_ERR_MEM_FULL);
	}
	*(void **) pblk = pmem->OSMemFreeList;	/*Èç¹ûÃ»Âú£¬µ±Ç°ÄÚ´æ¿éÖ¸Ïò¿ÕÏĞ¶ÓÁĞµÄµÚÒ»¸öÄÚ´æ¿é Insert released block into free block list         */
	pmem->OSMemFreeList = pblk;             //µ÷ÕûÖ¸Õë£¬½«ÊÍ·ÅµÄÄÚ´æ¿é·ÅÔÚ¿ÕÏĞÁ´±íµÄ¶ÓÊ×
	pmem->OSMemNFree++;	/*¼ÆÊıÖµ¸üĞÂ One more memory block in this partition            */
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
*               ²éÑ¯ÄÚ´æ·ÖÇøµÄ×´Ì¬
*
* Arguments   : pmem        is a pointer to the memory partition control block
*                           Ö¸ÏòÄÚ´æ·ÖÇøµÄÖ¸Õë
*
*               p_mem_data  is a pointer to a structure that will contain information about the memory
*                           partition.
*                           ±£´æ²éÑ¯½á¹û
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
	OS_CPU_SR cpu_sr = 0u;      //²ÉÓÃµÚÈıÖÖ·½Ê½¿ª¹ØÖĞ¶Ï£¬ĞèÒªcpu_srÀ´±£´æÖĞ¶Ï×´Ì¬
#endif



#if OS_ARG_CHK_EN > 0u
	if (pmem == (OS_MEM *) 0) {	/*È·±£Ö¸ÕëÖ¸ÏòµÄÄÚ´æ¿ØÖÆ¿éÊÇÓĞĞ§µÄ Must point to a valid memory partition             */
		return (OS_ERR_MEM_INVALID_PMEM);
	}
	if (p_mem_data == (OS_MEM_DATA *) 0) {	/*È·±£±£´æ²éÑ¯½á¹ûµÄÊı¾İ½á¹¹¿ÉÓÃ Must release a valid storage area for the data     */
		return (OS_ERR_MEM_INVALID_PDATA);
	}
#endif
	OS_ENTER_CRITICAL();                            //·ÀÖ¹ÖĞ¶Ï½øÈë¶øĞŞ¸ÄÄ³Ğ©±äÁ¿£¬ĞèÒª¹ØÖĞ¶Ï
	p_mem_data->OSAddr = pmem->OSMemAddr;           //²éÑ¯ÄÚ´æ·ÖÇøÆğÊ¼µØÖ·
	p_mem_data->OSFreeList = pmem->OSMemFreeList;   //²éÑ¯¿ÕÏĞÄÚ´æ¿éµÄÆğÊ¼µØÖ·
	p_mem_data->OSBlkSize = pmem->OSMemBlkSize;     //²éÑ¯Ã¿¸öÄÚ´æ¿éµÄ´óĞ¡
	p_mem_data->OSNBlks = pmem->OSMemNBlks;         //²éÑ¯ÄÚ´æ·ÖÇøÖĞÄÚ´æ¿é×ÜÊı
	p_mem_data->OSNFree = pmem->OSMemNFree;         //²éÑ¯ÄÚ´æ·ÖÇøÖĞ¿ÕÏĞµÄÄÚ´æ¿éÊıÄ¿
	OS_EXIT_CRITICAL();
	p_mem_data->OSNUsed = p_mem_data->OSNBlks - p_mem_data->OSNFree;    //¼ÆËãÒÑ¾­Ê¹ÓÃµÄÄÚ´æ¿éÊıÄ¿
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
*               ³õÊ¼»¯ÄÚ´æ¿ØÖÆ¿é
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
#if OS_MAX_MEM_PART == 1u                                   //Èç¹ûÖ»ÓĞÒ»¸öÄÚ´æ¿ØÖÆ¿é
	OS_MemClr((INT8U *) & OSMemTbl[0], sizeof(OSMemTbl));	/*ÇåÀí¿Õ¼ä Clear the memory partition table          */
	OSMemFreeList = (OS_MEM *) & OSMemTbl[0];	/*¿ÕÏĞÄÚ´æ¿é¿ØÖÆÁ´±íµÄÖ¸ÕëÖ¸ÏòÕâÎ¨Ò»µÄÒ»¸öÄÚ´æ¿ØÖÆ¿é Point to beginning of free list           */
#if OS_MEM_NAME_EN > 0u
	OSMemFreeList->OSMemName = (INT8U *) "?";	/*Ãû×ÖÄ¬ÈÏ Unknown name                              */
#endif
#endif

#if OS_MAX_MEM_PART >= 2u                                   //²»Ö¹Ò»¸öÄÚ´æ¿ØÖÆ¿é
	OS_MEM *pmem;
	INT16U i;


	OS_MemClr((INT8U *) & OSMemTbl[0], sizeof(OSMemTbl));	/*ÇåÀíËùÓĞÄÚ´æ¿ØÖÆ¿éÊı×éµÄ¿Õ¼ä Clear the memory partition table          */
	for (i = 0u; i < (OS_MAX_MEM_PART - 1u); i++) {	/*±éÀúÊı×éÖĞµÄËùÓĞÄÚ´æ¿ØÖÆ¿é£¬³ıÁË×îºóÒ»¸ö Init. list of free memory partitions      */
		pmem = &OSMemTbl[i];	/*»ñÈ¡µ±Ç°µÄÄÚ´æ¿ØÖÆ¿éµØÖ· Point to memory control block (MCB)       */
		pmem->OSMemFreeList = (void *) &OSMemTbl[i + 1u];	/*µ±Ç°µÄÄÚ´æ¿ØÖÆ¿éÖ¸ÏòÏÂÒ»¸öÄÚ´æ¿ØÖÆ¿é Chain list of free partitions             */
#if OS_MEM_NAME_EN > 0u
		pmem->OSMemName = (INT8U *) (void *) "?";           //Ãû×ÖÄ¬ÈÏ
#endif
	}
	pmem = &OSMemTbl[i];                //×îºóÒ»¸öÄÚ´æ¿ØÖÆ¿é
	pmem->OSMemFreeList = (void *) 0;	/*×îºóÒ»¸öÖ¸ÏòNULL Initialize last node                      */
#if OS_MEM_NAME_EN > 0u
	pmem->OSMemName = (INT8U *) (void *) "?";   //Ãû×ÖÄ¬ÈÏ
#endif

	OSMemFreeList = &OSMemTbl[0];	/*¿ÕÏĞÁ´±íÖ¸ÕëÖ¸ÏòµÚÒ»¸öÄÚ´æ¿ØÖÆ¿é Point to beginning of free list           */
#endif
}
#endif				/* OS_MEM_EN                                 */
