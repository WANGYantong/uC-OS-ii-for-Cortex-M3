#include "userroot.h"

INT8U  err;
OS_EVENT* Str_Q;
OS_EVENT* Node1_Semp;
OS_EVENT* Node2_Semp;
OS_EVENT* Node3_Semp;


void *MagGrp[N_MESSAGES];
char *buf1;
char *buf2;
char *buf3;

char *msg1="package type: To Node 1";
char *msg2="package type: To Node 2";
char *msg3="package type: To Node 3";
char *msg_broadcast="package type: Broadcast!";


static OS_STK Node1_stack[TASKSTACK];
static OS_STK Node2_stack[TASKSTACK];
static OS_STK Node3_stack[TASKSTACK];
static OS_STK Gateway_stack[TASKSTACK];

int main(void)
{
	DEV_HardwareInit();
    OSInit();

    Str_Q=OSQCreate(&MagGrp[0], (INT16U)N_MESSAGES);
    Node1_Semp=OSSemCreate(0);
    Node2_Semp=OSSemCreate(0);
    Node3_Semp=OSSemCreate(0);

	OSTaskCreate(Node1, (void*)NULL, &Node1_stack[TASKSTACK - 1], 3);
    OSTaskCreate(Node2, (void*)NULL, &Node2_stack[TASKSTACK - 1], 4);
    OSTaskCreate(Node3, (void*)NULL, &Node3_stack[TASKSTACK - 1], 5);
	OSTaskCreate(Gateway, (void*)NULL, &Gateway_stack[TASKSTACK - 1], 6);

	OSStart();

	return 0;
}

void Node1(void* p_arg)
{
    while(1)
    {
        OSSemPend(Node1_Semp, 0, &err);
        printf("\r\n Node 1 is active");
        buf1=OSQPend(Str_Q, 0, &err);
        printf("\r\n Node 1: get msg: %s",buf1);
        printf("\r\n Node 1: Time is %5d", OSTimeGet());
        printf("\r\n Node 1: sleeping");
        OSTimeDly(4000);
    }
}

void Node2(void* p_arg)
{
    while(1)
    {
        OSSemPend(Node2_Semp, 0, &err);
        printf("\r\n Node 2 is active");
        buf2=OSQPend(Str_Q, 0, &err);
        printf("\r\n Node 2: get msg: %s",buf2);
        printf("\r\n Node 2: Time is %5d", OSTimeGet());
        printf("\r\n Node 2: sleeping");
        OSTimeDly(2000);
    }
}

void Node3(void* p_arg)
{
    while(1)
    {
        OSSemPend(Node3_Semp, 0, &err);
        printf("\r\n Node 3 is active");
        buf3=OSQPend(Str_Q, 0, &err);
        printf("\r\n Node 3: get msg: %s",buf3);
        printf("\r\n Node 3: Time is %5d", OSTimeGet());
        printf("\r\n Node 3: sleeping");
        OSTimeDly(1000);
    }
}

void Gateway(void* p_arg)
{
    static INT8U time;
    while(1)
    {
        printf("\r\n/*********************************/");
				printf("\r\n Master is active.");
        printf("\r\n Master:Wake up! Node 1.");
        OSSemPost(Node1_Semp);
        printf("\r\n Master:Wake up! Node 2.");
        OSSemPost(Node2_Semp);
        printf("\r\n Master:Wake up! Node 3.");
        OSSemPost(Node3_Semp);

        printf("\r\n Master is transporting msg...");
        if(time==0)
        {
            OSQPost(Str_Q, msg_broadcast);
            OSQPost(Str_Q, msg_broadcast);
            OSQPost(Str_Q, msg_broadcast);
            time++;
        }
        else
        {
            OSQPost(Str_Q, msg1);
            OSQPost(Str_Q, msg2);
            OSQPost(Str_Q, msg3);
            time=0;
        }

        printf("\r\n Master: sleeping");
				printf("\r\n/*********************************/");
				printf("\r\n");
        OSTimeDly(8000);

    }
}
