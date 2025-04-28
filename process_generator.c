#include "headers.h"

void clearResources(int);
int msgq_id;

int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
    if (argc < 2)
    {
        printf("Invalid number of arguments!\n");
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
    int id, arrival, runtime, priority;
    fscanf(pFile,"%*[^\n]\n");
    printf("-----------------------------Processes------------------------------\n");
    int count = 1;
    while(fscanf(pFile,"%d\t%d\t%d\t%d\n",&id,&arrival,&runtime,&priority)==4)
    {
        printf("Process %d:\t", count);
        printf("%d\t%d\t%d\t%d\n",id,arrival,runtime,priority);
        PCB *readpcb = malloc(sizeof(PCB));
        printf("Testing ID: %d", id);
        readpcb->id = id;
        readpcb->arrival_time = arrival;
        readpcb->runtime = runtime;
        readpcb->priority = priority;
        enqueue(PCBs,readpcb); 
        count++;
    }
    fclose(pFile);
    //printQueue(PCBs);

    // 2. Read the chosen scheduling algorithm and its parameters, if there are any from the argument list.
    ///    1- Non-preemptive Highest Priority First (HPF)
    ///    2- Shortest Remaining Time Next (SRTN)
    ///    3- Round Robin (RR)
    int chosenAlgorithm;
    printf("Enter a scheduling algorithm:\n");
    printf("1 - Non-preemptive Highest Priority First (HPF)\n");
    printf("2 - Shortest Remaining Time Next (SRTN)\n");
    printf("3 - Round Robin (RR)\n");
    printf("Please enter the number corresponding to your choice: ");

    scanf("%d", &chosenAlgorithm);
    while (chosenAlgorithm != 1 && chosenAlgorithm != 2 && chosenAlgorithm != 3)
    {
        printf("Invalid algorithm number!\n");
        printf("Please enter the number corresponding to your choice: ");
        scanf("%d", &chosenAlgorithm);
    }

    int quantum;
    if (chosenAlgorithm == 3)
    {
        printf("Enter quantum: ");
        scanf("%d", &quantum);
        while (quantum < 0)
        {
            printf("Quantum must greater than or equal zero!\n");
            printf("Enter the quantum for Round Robin (RR): ");
            scanf("%d", &quantum);
        }
    }

    char chosenAlgorithm_str[2];
    char quantum_str[2];
    sprintf(chosenAlgorithm_str, "%d", chosenAlgorithm);
    sprintf(quantum_str, "%d", quantum);

    printf("You selected ");
    switch (chosenAlgorithm)
    {
    case 1:
        printf("Non-preemptive Highest Priority First (HPF)\n");
        break;
    case 2:
        printf("Shortest Remaining Time Next (SRTN)\n");
        break;
    case 3:   
        printf("Round Robin (RR) with quantum %d\n",quantum);
        break;
    default:
        break;
    }

    // 3. Initiate and create the scheduler and clock processes.
    int schedulerid=fork();
    if (schedulerid == -1) 
    {
        perror("Error forking scheduler!");
        exit(1);
    }
    if(schedulerid==0)
    {
        int compilestatus=system("gcc scheduler.c -o scheduler.out");
        if(compilestatus==0)
        {
            printf("Scheduler process started!\n");
            execl("./scheduler.out", "scheduler.out", chosenAlgorithm_str, quantum_str, NULL); // runing the scheduler with chosen algorithm and quantum
            printf("Error executing scheduler!");
            exit(1);
        }
        else
        {
            printf("Error in compiling scheduler!\n");
            exit(1);
        }
    }
  
    // 4. Use this function after creating the clock process to initialize clock.
    int clckid=fork();
    if (clckid == -1) 
    {
        perror("Error forking clock");
        exit(1);
    }
    if(clckid==0)
    {
        int clkcompilestatus=system("gcc clk.c -o clk.out");
        if(clkcompilestatus==0)
        {
            printf("Clock process started!\n");
            execl("./clk.out", "clk.out", NULL); // runing the clock
            printf("Error executing scheduler!");
            exit(1);
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
        process.pcb.start_time=-1;
        process.pcb.runtime=currentpcb->runtime;
        process.pcb.remaining_time=currentpcb->runtime;
        process.pcb.waiting_time=0;
        process.pcb.priority=currentpcb->priority;
        process.pcb.state=READY;
        while(getClk() < currentpcb->arrival_time);
        printf("Sending process %d with arrival time %d and runtime %d and priority %d\n",process.pcb.id,process.pcb.arrival_time,process.pcb.runtime,process.pcb.priority);
        printf("Current Clock Time: %d\n", getClk());
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
