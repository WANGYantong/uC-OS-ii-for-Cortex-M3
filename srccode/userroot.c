#include "userroot.h"

static OS_STK Task1_stack[TASKSTACK];
static OS_STK Task2_stack[TASKSTACK];

int main(void)
{
    char* s_task1="Freqchip_Shenzhen";
    char* s_task2="Freqchip_Shanghai";

	DEV_HardwareInit();
	OSInit();
	OSTaskCreate(Task1, s_task1, &Task1_stack[TASKSTACK - 1], 2);
	OSTaskCreate(Task2, s_task2, &Task2_stack[TASKSTACK - 1], 3);

	OSStart();

	return 0;
}

void Task1(void *p_arg)
{
	while (1) {
		OSTimeDly(1000);
		printf("Task 1 is running,location: %s!",(char *)p_arg);
		printf("\n");
	}
}

void Task2(void *p_arg)
{
	while (1) {
		printf("Task 2 is running,location: %s!",(char *)p_arg);
		printf("\n");
		OSTimeDly(1000);
	}
}
