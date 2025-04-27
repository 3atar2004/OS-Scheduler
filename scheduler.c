#include "headers.h"
int queueopen=1;
void terminategenerator(int signum);
void HPF();
void SRTN();
void RR(int quantum);
int main(int argc, char *argv[])
{
    initClk();
    signal(SIGUSR2, terminategenerator);
    int compileprocess=system("gcc process.c -o process.out");
    if(compileprocess!=0)
    {
        printf("Error in compiling process\n");
        exit(1);
    }
    key_t msg_id=ftok("msgqueue", 65);
    int msgq_id=msgget(msg_id, 0666|IPC_CREAT);

    while(queueopen)
    {
        msgbuff msg;
        int rec_val=msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), 1, !IPC_NOWAIT);
        printf("Recieved process %d at time %d and runtime %d and priority %d\n",msg.pcb.id,getClk(),msg.pcb.runtime,msg.pcb.priority);
    }
    // 
    switch(atoi(argv[1])) // 1- Non Preemptive HPF, 2- SRTN, 3- RR
    {
        case 1:
            HPF();
            break;
        case 2:
            SRTN();
            break;
        case 3:   
            RR(atoi(argv[2]));
            break;
        default:
            break;
    }

    //TODO: implement the scheduler.
    //TODO: upon termination release the clock resources.

    destroyClk(true);
    return(0);
}
void HPF()
{
    printf("Non Preemptive HPF\n");
}
void SRTN()
{
    printf("SRTN\n");
}
void RR(int quantum)
{
    printf("Round Robin with quantum %d\n",quantum);
}
void terminategenerator(int signum)
{
    queueopen=0; // indicator that the queue has closed and no more processes will be sent from generator
    destroyClk(false);
    exit(0);
}
