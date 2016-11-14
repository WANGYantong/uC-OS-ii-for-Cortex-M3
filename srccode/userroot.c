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


char *food1="¿�����";
char *food2="�����";
char *food3="����������";
char *food4="��ţ��";
char *food5="���ӷ�";
char *food6="±�ⷹ";

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
        printf("\r\n�˿�1����\r\n");
        buf1=(char*)OSQPend(Str_Q, 0, &err);
        printf("\r\n�˿�1�Ե���:%s\r\n",buf1);
        printf("\r\n�˿�1�Ա��ˣ�����\r\n");
				printf("\r\n\\11111111111111111111111111111\\\r\n");
        OSTimeDly(4000);
    }
}

void Customer2(void* p_arg)
{
    while(1)
        {
			  printf("\r\n\\22222222222222222222222222222\\\r\n");
        printf("\r\n�˿�2����\r\n");
        buf2=(char*)OSQPend(Str_Q, 0, &err);
        printf("\r\n�˿�2�Ե���:%s\r\n",buf2);
        printf("\r\n�˿�2�Ա��ˣ�����\r\n");
				printf("\r\n\\22222222222222222222222222222\\\r\n");
        OSTimeDly(2000);
    }

}

void Customer3(void* p_arg)
{
    while(1)
        {
				printf("\r\n\\33333333333333333333333333333\\\r\n");
        printf("\r\n�˿�3����\r\n");
        buf3=(char*)OSQPend(Str_Q, 0, &err);
        printf("\r\n�˿�3�Ե���:%s\r\n",buf3);
        printf("\r\n�˿�3�Ա��ˣ�����\r\n");
				printf("\r\n\\33333333333333333333333333333\\\r\n");
        OSTimeDly(1000);
    }

}

void Cook(void* p_arg)
{
    while(1)
        {
					  printf("\r\n\\**************************\\\r\n");
            printf("\r\n��ʦ��ʼ����\r\n");
            printf("\r\n��һ����: %s\r\n",food1);
            OSQPost(Str_Q, food1);
            printf("\r\n�ڶ�����: %s\r\n",food2);
            OSQPost(Str_Q, food2);
            printf("\r\n��������: %s\r\n",food3);
            OSQPost(Str_Q, food3);
            printf("\r\n���ĵ���: %s\r\n",food4);
            OSQPost(Str_Q, food4);
            printf("\r\n�������: %s\r\n",food5);
            OSQPost(Str_Q, food5);
            printf("\r\n��������: %s\r\n",food6);
            OSQPost(Str_Q, food6);
            printf("\r\n��ʦ��Ϣ\r\n");
					  printf("\r\n\\**************************\\\r\n");
            OSTimeDly(12000);
    }
}
