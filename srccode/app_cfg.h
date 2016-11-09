#ifndef __APP_CFG_H__
#define __APP_CFG_H__

#include <stdio.h>
#include "stm32f10x_conf.h"
#include "ucos_ii.h"

#define TASKSTACK                   512

extern void DEV_HardwareInit(void);

#endif
