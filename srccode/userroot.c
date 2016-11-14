#include "userroot.h"

INT8U  err;
OS_EVENT* Str_Q;
void *MagGrp[N_MESSAGES];
char *buf1;
char *buf2;
char *buf3;

static OS_STK Task1_stack[TASKSTACK];
static OS_STK Task2_stack[TASKSTACK];
static OS_STK Task3_stack[TASKSTACK];
static OS_STK Task4_stack[TASKSTACK];


char *food1="驴肉火烧";
char *food2="肉夹馍";
char *food3="猪肉炖粉条";
char *food4="酱牛肉";
char *food5="炒河粉";
char *food6="卤肉饭";

int main(void)
{
	DEV_HardwareInit();
    OSInit();
    Str_Q=OSQCreate(&MagGrp[0], (INT16U)N_MESSAGES);
	  OSTaskCreate(Customer1, (void*)NULL, &Task1_stack[TASKSTACK - 1], 2);
    OSTaskCreate(Customer2, (void*)NULL, &Task2_stack[TASKSTACK - 1], 3);
    OSTaskCreate(Customer3, (void*)NULL, &Task3_stack[TASKSTACK - 1], 4);
	  OSTaskCreate(Cook, (void*)NULL, &Task4_stack[TASKSTACK - 1], 5);

	OSStart();

	return 0;
}

void Customer1(void* p_arg)
{
    while(1)
        {
				printf("\r\n\\11111111111111111111111111111\\\r\n");
        printf("\r\n顾客1来了\r\n");
        buf1=(char*)OSQPend(Str_Q, 0, &err);
        printf("\r\n顾客1吃的是:%s\r\n",buf1);
        printf("\r\n顾客1吃饱了，走了\r\n");
				printf("\r\n\\11111111111111111111111111111\\\r\n");
        OSTimeDly(4000);
    }
}

void Customer2(void* p_arg)
{
    while(1)
        {
			  printf("\r\n\\22222222222222222222222222222\\\r\n");
        printf("\r\n顾客2来了\r\n");
        buf2=(char*)OSQPend(Str_Q, 0, &err);
        printf("\r\n顾客2吃的是:%s\r\n",buf2);
        printf("\r\n顾客2吃饱了，走了\r\n");
				printf("\r\n\\22222222222222222222222222222\\\r\n");
        OSTimeDly(2000);
    }

}

void Customer3(void* p_arg)
{
    while(1)
        {
				printf("\r\n\\33333333333333333333333333333\\\r\n");
        printf("\r\n顾客3来了\r\n");
        buf3=(char*)OSQPend(Str_Q, 0, &err);
        printf("\r\n顾客3吃的是:%s\r\n",buf3);
        printf("\r\n顾客3吃饱了，走了\r\n");
				printf("\r\n\\33333333333333333333333333333\\\r\n");
        OSTimeDly(1000);
    }

}

void Cook(void* p_arg)
{
    while(1)
        {
					  printf("\r\n\\**************************\\\r\n");
            printf("\r\n厨师开始做饭\r\n");
            printf("\r\n第一道菜: %s\r\n",food1);
            OSQPost(Str_Q, food1);
            printf("\r\n第二道菜: %s\r\n",food2);
            OSQPost(Str_Q, food2);
            printf("\r\n第三道菜: %s\r\n",food3);
            OSQPost(Str_Q, food3);
            printf("\r\n第四道菜: %s\r\n",food4);
            OSQPost(Str_Q, food4);
            printf("\r\n第五道菜: %s\r\n",food5);
            OSQPost(Str_Q, food5);
            printf("\r\n第六道菜: %s\r\n",food6);
            OSQPost(Str_Q, food6);
            printf("\r\n厨师休息\r\n");
					  printf("\r\n\\**************************\\\r\n");
            OSTimeDly(12000);
    }
}
