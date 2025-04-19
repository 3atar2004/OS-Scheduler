#include "headers.h"

void clearResources(int);
int msgq_id;

int main(int argc, char *argv[])
{
    //destroyClk(true);
    signal(SIGINT, clearResources);
    // TODO Initialization
    if(argc<4)
    {
        printf("incorrect number of arguments\n");
        exit(1);
    }
     // 1. Read the input files.
    CircularQueue *PCBs = (CircularQueue *)malloc(sizeof(CircularQueue));
    initQueue(PCBs);
    FILE *pFile;
    pFile = fopen(argv[1], "r");
    if (pFile == NULL)
    {
        printf("Error opening file\n");
        exit(1);
    }
    int id,arrival,runtime,priority;
    fscanf(pFile,"%*[^\n]\n");
    while(fscanf(pFile,"%d\t%d\t%d\t%d\n",&id,&arrival,&runtime,&priority)==4)
    {
        printf("%d\t%d\t%d\t%d\n",id,arrival,runtime,priority);
        PCB *readpcb = malloc(sizeof(PCB));
        readpcb->id = id;
        readpcb->arrival_time = arrival;
        readpcb->runtime = runtime;
        readpcb->priority = priority;
        enqueue(PCBs,readpcb); 
    }
    fclose(pFile);
    //printQueue(PCBs);

    // 2. Read the chosen scheduling algorithm and its parameters, if there are any from the argument list.
    ///    1- non preemptive HPF
    ///    2- SRTN
    ///    3- RR
    int chosenalgorithm=atoi(argv[2]);
    int quantum=atoi(argv[3]);
    printf("You chose:");
    switch (chosenalgorithm)
    {
    case 1:
        printf("Non Preemptive HPF\n");
        break;
    case 2:
        printf("SRTN\n");
        break;
    case 3:   
        printf("Round Robin with quantum %d\n",quantum);
        if(quantum<=0)
        {
            printf("Quantum must be greater than 0\n");
            exit(1);
        }
        break;
    
    default:
        break;
    }

    // 3. Initiate and create the scheduler and clock processes.
    int schedulerid=fork();
    if(schedulerid==0)
    {
        int compilestatus=system("gcc scheduler.c -o scheduler.out");
        if(compilestatus==0)
        {
            execl("./scheduler.out", "scheduler.out", argv[2], argv[3], NULL); // runing the scheduler with chosen algorithm and quantum
            printf("Error in execl\n");

        }
        else
        {
            printf("Error in compiling scheduler\n");
            exit(1);
        }
    }
  
    // 4. Use this function after creating the clock process to initialize clock.
    int clckid=fork();
    if(clckid==0)
    {
        int clkcompilestatus=system("gcc clk.c -o clk.out");
        if(clkcompilestatus==0)
        {
            execl("./clk.out", "clk.out", NULL); // runing the clock
            printf("Error in execl\n");
        }
        else
        {
            printf("Error in compiling clk\n");
            exit(1);
        }
    }
    initClk();
    // To get time use this function. 
     int x = getClk();
     printf("Current Time is %d\n", x);
    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters.
    // 6. Send the information to the scheduler at the appropriate time.
    key_t msg_id;
    int send_val;
    msg_id=ftok("msgqueue", 65);
    int msgq_id=msgget(msg_id, IPC_CREAT | 0666);
    if(msgq_id==-1)
    {
        printf("Error in creating message queue\n");
        exit(-1);
    }
    struct msgbuff process;
     while(!isEmpty(PCBs))
     {
        PCB* currentpcb;
        dequeue(PCBs,&currentpcb);
        process.pcb.id=currentpcb->id;
        process.pcb.arrival_time=currentpcb->arrival_time;
        process.pcb.runtime=currentpcb->runtime;
        process.pcb.remaining_time=currentpcb->runtime;
        process.pcb.waiting_time=0;
        process.pcb.priority=currentpcb->priority;
        process.pcb.state=READY;
        while(getClk() < currentpcb->arrival_time);
        printf("Sending process %d with arrival time %d and runtime %d and priority %d\n",process.pcb.id,process.pcb.arrival_time,process.pcb.runtime,process.pcb.priority);
        process.mtype=1;
        send_val=msgsnd(msgq_id,&process,sizeof(process.pcb),!IPC_NOWAIT);
        if(send_val==-1)
        {
            printf("Error in sending message\n");
        }
        free(currentpcb);
     }
    kill(schedulerid, SIGUSR2); // send signal that i won't be sending any proccesses.
    int stat;
    wait(&stat); // wait for scheduler to finish


    // 7. Clear clock resources
    msgctl(msgq_id, IPC_RMID, (struct msqid_ds *)0);
    destroyClk(true);
}

void clearResources(int signum)
{
    destroyClk(true);
    msgctl(msgq_id, IPC_RMID, (struct msqid_ds *)0);
    raise(SIGKILL);
}
