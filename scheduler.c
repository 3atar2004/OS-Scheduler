#include "headers.h"
#include <unistd.h>
#include <signal.h>
int queueopen=1;
int currentTime = 0;
int numberOfProcesses = 0;
float avgWTA;
float avgWaiting;
float stdWTA;
int totalRunTime = 0;
int totalWaitTime = 0;
float sumTA = 0;
float sumWTA = 0;

void terminategenerator(int signum);
void finishedProcess(int signum);
void HPF();
void SRTN();
void RR(int quantum);

int msgq_id;
CircularQueue *readyQueue;
FILE *fptr;

int main(int argc, char *argv[])
{
    initClk();
    signal(SIGUSR1, finishedProcess);
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

    readyQueue = (CircularQueue *)malloc(sizeof(CircularQueue));
    initQueue(readyQueue);

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
    fflush(fptr);

    msgbuff msg;
    int status = msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), 1, !IPC_NOWAIT); // Scheduler won't start untill first process received
    if (status != -1) // The first process has arrvied
    {
        PCB *arrivedPCB = malloc(sizeof(PCB));
        memcpy(arrivedPCB, &msg.pcb, sizeof(PCB));
        enqueue(readyQueue, arrivedPCB);
        printf("Scheduler received process %d at time %d with runtime %d and priority %d \n", arrivedPCB->id, getClk(), arrivedPCB->runtime, arrivedPCB->priority);
        numberOfProcesses++;
    }   
    
    // Now round robin main algorithm is here
    PCB *runningProcess = NULL;
    int currentQuantum = 0;
    // First need to check if there is work to do
    // Process generator sets the flag queueopen = 0 to notify scheduler that is done with sending processes
    while (!isEmpty(readyQueue) || queueopen == 1 || runningProcess != NULL)
    {
        int currentTime = getClk();
        
        int status = msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), 1, IPC_NOWAIT); // Check for new arrivals from generator
        if (status != -1) // No new arrivals
        {
            // New arrivals are in queue
            PCB *arrivedPCB = malloc(sizeof(PCB));
            memcpy(arrivedPCB, &msg.pcb, sizeof(PCB));
            enqueue(readyQueue, arrivedPCB);
            printf("Scheduler received process %d at time %d with runtime %d and priority %d \n", arrivedPCB->id, getClk(), arrivedPCB->runtime, arrivedPCB->priority);
            numberOfProcesses++;
        }

        if (!runningProcess) // If there is currently no running process
        {
            // Need to check if ready queue is empty;
            if (isEmpty(readyQueue)) continue;
            else // There are processes in queue
            {
                dequeue(readyQueue, &runningProcess);
                if (runningProcess->pid == -1) // Hasn't run before (Hasn't been forked before)
                {
                    // First time starter
                    runningProcess->state = STARTED;
                    char stateString[10];
                    strcpy(stateString, stateStrings[runningProcess->state]);
                    runningProcess->start_time = getClk();
                    runningProcess->waiting_time = runningProcess->start_time - runningProcess->arrival_time;
                    totalWaitTime += runningProcess->waiting_time;

                    fprintf(fptr, "At time %d process %d %s arr %d total %d remain %d wait %d\n", getClk(), runningProcess->id, stateString, runningProcess->arrival_time, runningProcess->runtime, runningProcess->remaining_time, runningProcess->waiting_time);
                    fflush(fptr);

                    int runningProcessID = fork();
                    // Parent is scheduler and child is process
                    if (runningProcessID == 0) // Send process for running
                    {
                        int schedulerID = getppid();
                        char schedulerID_str[20];
                        sprintf(schedulerID_str, "%d", schedulerID);
                        int runtime = runningProcess->runtime;
                        char runtime_str[5];
                        sprintf(runtime_str, "%d", runtime);
                        int processID = runningProcess->id;
                        char processID_str[5];
                        sprintf(processID_str, "%d", processID);
                        int processCompileStatus = system("gcc process.c -o process.out");
                        if (processCompileStatus == 0)
                        {
                            execl("./process.out", "process.out", runtime_str, schedulerID_str, processID_str, NULL);
                            printf("Error executing process\n!");
                            exit(1);
                        }
                        else
                        {
                            printf("Error in compiling process!\n");
                            exit(1);
                        }
                    }
                    else // Scheduler sets the process id after forking
                    {
                        runningProcess->pid = runningProcessID;
                    }
                }
                else // It has previously been forked meaning has ran before
                {
                    runningProcess->state = RESUMED;
                    char stateString [10];
                    strcpy(stateString, stateStrings[runningProcess->state]);
                    runningProcess->restarted_time = getClk();
                    totalWaitTime -= runningProcess->waiting_time; // Removed old waiting time from total waiting time
                    runningProcess->waiting_time += runningProcess->restarted_time - runningProcess->stopped_time; // Update process waiting time
                    totalWaitTime += runningProcess->waiting_time; // Now add updated waiting time to total waiting time
                    fprintf(fptr, "At time %d process %d %s arr %d total %d remain %d wait %d\n", getClk(), runningProcess->id, stateString, runningProcess->arrival_time, runningProcess->runtime, runningProcess->remaining_time, runningProcess->waiting_time);
                    fflush(fptr);
                    
                    kill(runningProcess->pid, SIGCONT);
                }
            }
            currentQuantum = 0;
        }

        if (runningProcess)
        {
            currentQuantum++;
            runningProcess->remaining_time--; // Decrement remaining time by 1 quantum
            while (getClk() <= currentTime);
            if (runningProcess->remaining_time == 0) // This is process is finished
            {
                runningProcess->state = FINISHED;
                char stateString [20];
                strcpy(stateString, stateStrings[FINISHED]);
                runningProcess->finished_time = getClk();
                int TA = runningProcess->finished_time - runningProcess->arrival_time;
                float WTA = TA / (float)runningProcess->runtime;
                sumTA += TA;
                sumWTA += WTA;
                fprintf(fptr, "At time %d process %d %s arr %d total %d remain %d wait %d TA %d WTA %.2f\n", getClk(), runningProcess->id, stateString, runningProcess->arrival_time, runningProcess->runtime, runningProcess->remaining_time, runningProcess->waiting_time, TA, WTA);
                fflush(fptr);
                runningProcess = NULL;
            }
            
            // If process didn't finish in the total quantum then it still has remaining time so need to be stopped
            if (runningProcess != NULL && currentQuantum >= quantum)
            {
                runningProcess->state = STOPPED;
                char stateString [20];
                strcpy(stateString, stateStrings[STOPPED]);
                runningProcess->stopped_time = getClk();
                enqueue(readyQueue, runningProcess);
                fprintf(fptr, "At time %d process %d %s arr %d total %d remain %d wait %d\n", getClk(), runningProcess->id, stateString, runningProcess->arrival_time, runningProcess->runtime, runningProcess->remaining_time, runningProcess->waiting_time);
                fflush(fptr);
                kill(runningProcess->pid, SIGSTOP);
                runningProcess = NULL;
                currentQuantum = 0;
            }
        }
    }
    fclose(fptr);
}

void finishedProcess(int signum)
{
    return;
    // To be done later
}
void terminategenerator(int signum)
{
    queueopen = 0; // indicator that the queue has closed and no more processes will be sent from generator
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