#include "device.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/**********************************************/
//函数功能:初始化硬件
//输入参数:none
//返回值  :none
/**********************************************/
void DEV_HardwareInit(void)
{
	SysTick_Config(72000000 / OS_TICKS_PER_SEC);
	DEV_UartInit();
}

/**********************************************/
//函数功能:初始化串口
//输入参数:none
//返回值  :none
/**********************************************/
void DEV_UartInit(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1 | RCC_APB2Periph_AFIO, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	USART_StructInit(&USART_InitStructure);
	USART_Init(USART1, &USART_InitStructure);
	USART_Cmd(USART1, ENABLE);

}

/**********************************************/
//函数功能:向串口发送一个字符
//输入参数:ucChar:待输出的字符
//返回值  :none
/**********************************************/
void DEV_PutChar(INT8U ucChar)
{
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
	USART_SendData(USART1, ucChar);
}
