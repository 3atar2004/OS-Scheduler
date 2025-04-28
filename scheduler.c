#include "headers.h"
int queueopen=1;
void terminategenerator(int signum);
void HPF();
void SRTN();
void RR(int quantum);

int msgq_id;
CircularQueue *PCBs;
FILE *fptr;

int main(int argc, char *argv[])
{
    initClk();
    signal(SIGUSR2, terminategenerator);
    int compileprocess=system("gcc process.c -o process.out");
    if (compileprocess != 0)
    {
        printf("Error in compiling process\n");
        exit(1);
    }
    
    int chosenAlgorithm = atoi(argv[1]);
    int quantum = atoi(argv[2]);
    key_t msg_id=ftok("msgqueue", 65);
    msgq_id=msgget(msg_id, 0666|IPC_CREAT);

    PCBs = (CircularQueue *)malloc(sizeof(CircularQueue));
    initQueue(PCBs);

    // while(queueopen)
    // {
    //     msgbuff msg;
    //     int rec_val=msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), 1, !IPC_NOWAIT);
    //     printf("Recieved process %d at time %d and runtime %d and priority %d\n",msg.pcb.id,getClk(),msg.pcb.runtime,msg.pcb.priority);
    // }

    switch(chosenAlgorithm) // 1- Non Preemptive HPF, 2- SRTN, 3- RR
    {
        case 1:
            HPF();
            break;
        case 2:
            SRTN();
            break;
        case 3:   
            RR(quantum);
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
    fptr = fopen("scheduler.log", "w");
    if (!fptr)
    {
        printf("Error opening scheduler.log\n");
        return;
    }
    fprintf(fptr, "#At  time  x  process  y  state  arr  w  total  z  remain  y  wait  k\n");
    
    PCB *runningPCB;
    while (1)
    {
        msgbuff msg;
        int status = msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), 1, IPC_NOWAIT);
        if (status != -1) // A new process has arrived
        {
            PCB *arrivedPCB = malloc(sizeof(PCB));
            arrivedPCB->arrival_time = msg.pcb.arrival_time;
            arrivedPCB->finished_time = msg.pcb.finished_time;
            arrivedPCB->id = msg.pcb.id;
            arrivedPCB->pid = msg.pcb.pid;
            arrivedPCB->priority = msg.pcb.priority;
            arrivedPCB->remaining_time = msg.pcb.remaining_time;
            arrivedPCB->remainingTimeAfterStop = msg.pcb.remainingTimeAfterStop;
            arrivedPCB->restarted_time = msg.pcb.restarted_time;
            arrivedPCB->runtime = msg.pcb.runtime;
            arrivedPCB->start_time = msg.pcb.start_time;
            arrivedPCB->state = msg.pcb.state;
            arrivedPCB->stopped_time = msg.pcb.stopped_time;
            arrivedPCB->waiting_time = msg.pcb.waiting_time;
            enqueue(PCBs, arrivedPCB); // Put process at end of queue
        }

        if (queueopen) // If still expecting new processes from generator
        {
            if (isEmpty(PCBs)) continue; // No processes are available at the moment to run
        }
        else if (!queueopen) // If no more expecting new processes
        {
            if (isEmpty(PCBs)) break;
        }

        dequeue(PCBs, &runningPCB);

        runningPCB->state = RUNNING;
        int currentTime = getClk();
        int processID = runningPCB->id;
        char processState [15];
        int arrivalTime = runningPCB->arrival_time;
        int totalRunningTime = runningPCB->runtime;
        int remainingRunningTime = runningPCB->remaining_time;
        int waitingTime = runningPCB->waiting_time;

        if (runningPCB->start_time == -1) // Check if process hasn't been run before (first timer)
        {
            runningPCB->start_time = getClk();
            strcpy(processState, stateStrings[STARTED]); // Get string equivalent of state 
            fprintf(fptr, "At  time  %d  process  %d  %s  arr  %d  total  %d  remain %d  wait %d\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime);
            printf("At  time  %d  process  %d  %s  arr  %d  total  %d  remain %d  wait %d\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime);
        }
        else // This process has run before (will resume)
        {
            strcpy(processState, stateStrings[RESUMED]); // Get string equivalent of state 
            fprintf(fptr, "At  time  %d  process  %d  %s  arr  %d  total  %d  remain %d  wait %d\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime);
            printf("At  time  %d  process  %d  %s  arr  %d  total  %d  remain %d  wait %d\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime);
        }

        int readyProcessesWaitingTime = quantum;
        // Now processing the current process
        if (remainingRunningTime < quantum) // Process will not take whole quantum
        {
            readyProcessesWaitingTime = remainingRunningTime;
            remainingRunningTime = 0; // Now process is finished
        }
        else // Process will take whole quantum
        {
            remainingRunningTime -= quantum;
        }

        runningPCB->remainingTimeAfterStop = remainingRunningTime;

        // Need to no adjust waiting times for all the processes in the ready qeueue
        updateWaitingTimes(PCBs, readyProcessesWaitingTime);
        
        // Now scheduler is done with this process
        // Need to check if process is finished or will be stopped

        if (runningPCB->remainingTimeAfterStop == 0) // Process is finished (No need to bring back to queue)
        {
            strcpy(processState, stateStrings[FINISHED]); // Get string equivalent of state
            runningPCB->finished_time = getClk(); 
            
            // Turnaround time = Finish time - Arrival time
            int TA = runningPCB->finished_time - runningPCB->arrival_time;
            float WTA = (float)TA / runningPCB->runtime;

            fprintf(fptr, "At  time  %d  process  %d  %s  arr  %d  total  %d  remain  %d  wait  %d  TA  %d  WTA  %f\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime, TA, WTA);
            fprintf("At  time  %d  process  %d  %s  arr  %d  total  %d  remain  %d  wait  %d  TA  %d  WTA  %f\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime, TA, WTA);
            free(runningPCB);
            continue;
        }
        else // Process will be stopped (Will be put back at rear of queue)
        {
            currentTime += readyProcessesWaitingTime;
            strcpy(processState, stateStrings[STOPPED]);
            fprintf(fptr, "At  time  %d  process  %d  %s  arr  %d  total  %d  remain %d  wait %d\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime);
            fprintf("At  time  %d  process  %d  %s  arr  %d  total  %d  remain %d  wait %d\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime);
        }

        // Now need to bring back process at rear of queue
        runningPCB->state = READY;
        enqueue(PCBs, runningPCB);
    }
    
    fclose(fptr);
}
void terminategenerator(int signum)
{
    queueopen=0; // indicator that the queue has closed and no more processes will be sent from generator
    destroyClk(false);
    exit(0);
}

void updateWaitingTimes(CircularQueue *q, int time) // This is only used on the ready queue
{
    if (q->rear == NULL) return;
    Node *current = q->rear->next;
    while (1) 
    {
        current->pcb->waiting_time += time;

        if (current == q->rear) break;

        current = current->next;
    }

}