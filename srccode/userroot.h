
#ifndef __USERROOT_H__
#define __USERROOT_H__

#include "app_cfg.h"

#define N_MESSAGES      128


void Customer1(void *p_arg);
void Customer2(void *p_arg);
void Customer3(void *p_arg);
void Cook(void *p_arg);

extern OS_EVENT* OSQCreate(void * * start, INT16U size);
extern void* OSQPend(OS_EVENT * pevent, INT32U timeout, INT8U * perr);
extern INT8U OSQPost(OS_EVENT * pevent, void * pmsg);

#endif
