#include "headers.h"
#include <unistd.h>
int queueopen=1;
int currentTime = 0;
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
    printf("Ana wasalt ba3d el clock!");
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
            *arrivedPCB = msg.pcb;
            enqueue(PCBs, arrivedPCB); // Put process at end of queue
        }

        if (queueopen) // If still expecting new processes from generator
        {
            if (isEmpty(PCBs))
            {
                sleep(1);
                currentTime++;
            }
        }
        else if (!queueopen) // If no more expecting new processes
        {
            if (status == -1 && isEmpty(PCBs)) // No more messages and queue is empty
            {
                break;
            }
        }

        dequeue(PCBs, &runningPCB);

        runningPCB->state = RUNNING;
        int processID = runningPCB->id;
        int arrivalTime = runningPCB->arrival_time;
        int totalRunningTime = runningPCB->runtime;
        int remainingRunningTime = runningPCB->remaining_time;
        int waitingTime = runningPCB->waiting_time;

        char processState [15];

        if (runningPCB->start_time == -1) // Check if process hasn't been run before (first timer)
        {
            runningPCB->start_time = currentTime;
            strcpy(processState, stateStrings[STARTED]); // Get string equivalent of state 
            fprintf(fptr, "At  time  %d  process  %d  %s  arr  %d  total  %d  remain %d  wait %d\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime);
            //printf("At  time  %d  process  %d  %s  arr  %d  total  %d  remain %d  wait %d\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime);
        }
        else // This process has run before (will resume)
        {
            strcpy(processState, stateStrings[RESUMED]); // Get string equivalent of state 
            fprintf(fptr, "At  time  %d  process  %d  %s  arr  %d  total  %d  remain %d  wait %d\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime);
            //printf("At  time  %d  process  %d  %s  arr  %d  total  %d  remain %d  wait %d\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime);
        }

        int processingTime = quantum;
        // Now processing the current process
        if (remainingRunningTime < quantum) // Process will not take whole quantum and will finish
        {
            processingTime = remainingRunningTime;
            remainingRunningTime = 0; // Now process is finished
        }
        else // Process will take whole quantum
        {
            remainingRunningTime -= processingTime;
        }

        runningPCB->remaining_time = remainingRunningTime; // Adjust remaining time after processing

        // Need to now adjust waiting times for all the processes in the ready qeueue
        updateWaitingTimes(PCBs, processingTime);
        
        sleep(processingTime); // To syncronize clock

        // Now scheduler is done with this process
        // Need to check if process is finished or will be stopped
        currentTime += processingTime; // update time after process is done with cpu

        if (runningPCB->remaining_time == 0) // Process is finished (No need to bring back to queue)
        {
            strcpy(processState, stateStrings[FINISHED]); // Get string equivalent of state
            runningPCB->finished_time = currentTime + processingTime; 
            
            // Turnaround time = Finish time - Arrival time
            int TA = runningPCB->finished_time - runningPCB->arrival_time;
            float WTA = (float)TA / runningPCB->runtime;

            fprintf(fptr, "At  time  %d  process  %d  %s  arr  %d  total  %d  remain  %d  wait  %d  TA  %d  WTA  %f\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime, TA, WTA);
            //fprintf("At  time  %d  process  %d  %s  arr  %d  total  %d  remain  %d  wait  %d  TA  %d  WTA  %f\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime, TA, WTA);
            free(runningPCB);
            continue;
        }
        else // Process will be stopped (Will be put back at rear of queue)
        {
            strcpy(processState, stateStrings[STOPPED]);
            fprintf(fptr, "At  time  %d  process  %d  %s  arr  %d  total  %d  remain %d  wait %d\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime);
            runningPCB->state = READY;
            enqueue(PCBs, runningPCB);
            continue;
            //fprintf("At  time  %d  process  %d  %s  arr  %d  total  %d  remain %d  wait %d\n", currentTime, processID, processState, arrivalTime, totalRunningTime, remainingRunningTime, waitingTime);
        }
    }
    
    fclose(fptr);
}
void terminategenerator(int signum)
{
    queueopen=0; // indicator that the queue has closed and no more processes will be sent from generator
    destroyClk(false);
    //exit(0);
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