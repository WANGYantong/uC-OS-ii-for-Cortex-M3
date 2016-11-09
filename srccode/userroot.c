#include "userroot.h"

static OS_STK Task1_stack[TASKSTACK];
static OS_STK Task2_stack[TASKSTACK];

int main(void)
{
	DEV_HardwareInit();
	OSInit();

	OSTaskCreate(Task1, (void *) NULL, &Task1_stack[TASKSTACK - 1], 2);
	OSTaskCreate(Task2, (void *) NULL, &Task2_stack[TASKSTACK - 1], 3);

	OSStart();

	return 0;
}

void Task1(void *p_arg)
{
	(void) p_arg;

	while (1) {
		OSTimeDly(1000);
		printf("Task 1 is running!");
		printf("\n");
	}
}

void Task2(void *p_arg)
{
	(void) p_arg;

	while (1) {
		printf("Task 2 is running!");
		printf("\n");
		OSTimeDly(1000);
	}
}
