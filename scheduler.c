#include "headers.h"
#include <unistd.h>
#include <signal.h>

int queueopen = 1;
int currentTime = 0;
int numberOfProcesses = 0;
float avgWTA;
float avgWaiting;
float stdWTA;
int totalRunTime = 0;
int totalWaitTime = 0;
float sumTA = 0;
float sumWTA = 0;
float WTA_values[100]; // Assume max 100 processes for now
int finished_index = 0;

void terminategenerator(int signum);
void finishedProcess(int signum);
void enqueueByRemainingTime(CircularQueue *q, PCB *pcb);
void peek(CircularQueue *q, PCB **pcb);
void HPF();
void SRTN();
void RR(int quantum);
float calculateStdWTA(float WTA_values[], int count, float avgWTA);
void receiveNewProcessBlocking();
void receiveNewProcessesNonBlocking();

int msgq_id;
CircularQueue *readyQueue;
FILE *fptr;

int main(int argc, char *argv[])
{
    initClk();
    signal(SIGUSR1, finishedProcess);
    signal(SIGUSR2, terminategenerator);
    int compileprocess = system("gcc process.c -o process.out");
    if (compileprocess != 0)
    {
        printf("Error in compiling process\n");
        exit(1);
    }

    int chosenAlgorithm = atoi(argv[1]);
    int quantum = atoi(argv[2]);
    key_t msg_id = ftok("msgqueue", 65);
    msgq_id = msgget(msg_id, 0666 | IPC_CREAT);

    readyQueue = (CircularQueue *)malloc(sizeof(CircularQueue));
    initQueue(readyQueue);

    // while(queueopen)
    // {
    //     msgbuff msg;
    //     int rec_val=msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), 1, !IPC_NOWAIT);
    //     printf("Recieved process %d at time %d and runtime %d and priority %d\n",msg.pcb.id,getClk(),msg.pcb.runtime,msg.pcb.priority);
    // }

    switch (chosenAlgorithm) // 1- Non Preemptive HPF, 2- SRTN, 3- RR
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

    // TODO: implement the scheduler.
    // TODO: upon termination release the clock resources.

    destroyClk(true);
    return (0);
}
void HPF()
{
    PriorityQueue *readyQueue = createQueue(); // Priority queue for processes
    PriorityQueue *WaitingQueue = createQueue();
    PCB *current_process = NULL;
    FILE *logfile = fopen("scheduler.log", "w");
    msgbuff receivedPCBbuff;
    int msgq_id;
    int totalWaitTime = 0, totalTurnaroundTime = 0, totalProcesses = 0, status;
    float WTA_sum = 0, WTA = 0;
    float runtime_sum = 0;
    key_t msg_id = ftok("msgqueue", 65);
    msgq_id = msgget(msg_id, 0666 | IPC_CREAT);
    if (msgq_id == -1)
    {
        perror("Error while creating the message queue for HPF");
        exit(-1);
    }

    // status = msgrcv(msgq_id, &receivedPCBbuff, sizeof(receivedPCBbuff) - sizeof(long), 1, !IPC_NOWAIT); // Scheduler won't start untill first process received
    // while (status != -1) // The first process has arrvied
    // {
    //     PCB *arrivedPCB = malloc(sizeof(PCB));
    //     memcpy(arrivedPCB, &receivedPCBbuff.pcb, sizeof(PCB));
    //     enqueuePri(readyQueue, arrivedPCB, arrivedPCB->priority);
    //     printf("Scheduler received process %d at time %d with runtime %d and priority %d \n", arrivedPCB->id, getClk(), arrivedPCB->runtime, arrivedPCB->priority);
    //     totalProcesses++;
    // }
    // Initialize the clock and get the starting time
    int clockTime = getClk();

    fprintf(logfile, "#At time x process y state arr w total z remain y wait k\n");

    while (!isPriEmpty(readyQueue) || current_process != NULL || queueopen == 1)
    {
        // Check for new processes in the message queue
        status = msgrcv(msgq_id, &receivedPCBbuff, sizeof(receivedPCBbuff) - sizeof(long), 1, IPC_NOWAIT); // Check for new arrivals from generator
        while (status != -1)                                                                               // No new arrivals
        {
            // New arrivals are in queue
            PCB *arrivedPCB = malloc(sizeof(PCB));
            memcpy(arrivedPCB, &receivedPCBbuff.pcb, sizeof(PCB));
            enqueuePri(readyQueue, arrivedPCB, arrivedPCB->priority);
            printf("Scheduler received process %d at time %d with runtime %d and priority %d \n", arrivedPCB->id, getClk(), arrivedPCB->runtime, arrivedPCB->priority);
            totalProcesses++;
            status = msgrcv(msgq_id, &receivedPCBbuff, sizeof(receivedPCBbuff) - sizeof(long), 1, IPC_NOWAIT); // Check for new arrivals from generator
        }

        // Handle the currently running process
        if (current_process)
        {
            if ((current_process->remaining_time) == 1)
            {
                // Process finished
                fprintf(logfile, "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n",
                        getClk() - 1, current_process->id, current_process->arrival_time, current_process->runtime,
                        current_process->waiting_time, clockTime - current_process->arrival_time,
                        (float)(clockTime - current_process->arrival_time) / current_process->runtime);

                totalWaitTime += current_process->waiting_time;
                totalTurnaroundTime += getClk() - current_process->arrival_time;
                WTA_sum += (float)(getClk() - current_process->arrival_time) / current_process->runtime;
                totalProcesses++;

                free(current_process);
                current_process = NULL;

                // Check waiting queue for processes that can now fit in memory
            }
            else
            {
                // In non-preemptive, we just decrement remaining time
                current_process->remaining_time--;
            }
        }

        // Start a new process if none is running
        if (!current_process && !isPriEmpty(readyQueue))
        {
            dequeuePri(readyQueue, &current_process);

            // Start a new process
            current_process->remaining_time = current_process->runtime;
            runtime_sum += current_process->runtime;
            current_process->waiting_time += (getClk() - current_process->arrival_time);
            current_process->pid = fork();
            if (current_process->pid == 0)
            {
                while (1)
                    pause(); // Waiting for SIGCONT
            }

            fprintf(logfile, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                    getClk(), current_process->id, current_process->arrival_time, current_process->runtime,
                    current_process->remaining_time, current_process->waiting_time);
        }

        // Advance the clock
        while (getClk() < clockTime);
        clockTime++;
    }

    // Log performance metrics
    float cpu_utilization = (runtime_sum / (getClk() - 1)) * 100;
    float avgWTA = WTA_sum / totalProcesses;
    float avgWaiting = (float)totalWaitTime / totalProcesses;
    FILE *perf;
    perf = fopen("scheduler.perf", "w");
    fprintf(perf, "CPU utilization = %.2f %% \n", cpu_utilization);
    fprintf(perf, "Avg WTA = %.2f \n", avgWTA);
    fprintf(perf, "Avg Waiting = %.2f \n", avgWaiting);
    fclose(perf);
    fclose(logfile);

    freePriQueue(readyQueue);
    freePriQueue(WaitingQueue);
    destroyClk(true);
}
void SRTN()
{
    fptr = fopen("scheduler.log", "w");
    if (!fptr)
    {
        printf("Error opening scheduler.log\n");
        return;
    }
    fprintf(fptr, "#At time x process y state arr w total z remain y wait k\n");
    fflush(fptr);

    msgbuff msg;
    PCB *runningProcess = NULL;
    int lastTime = -1;

    if (msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), 1, 0) != -1)
    {
        PCB *newPCB = malloc(sizeof(PCB));
        memcpy(newPCB, &msg.pcb, sizeof(PCB));
        enqueueByRemainingTime(readyQueue, newPCB);
        numberOfProcesses++;
    }

    while (!isEmpty(readyQueue) || queueopen == 1 || runningProcess != NULL)
    {
        int currentTime = getClk();
        if (currentTime == lastTime)
            continue;
        lastTime = currentTime;

        // Receive new arrivals
        while (msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), 1, IPC_NOWAIT) != -1)
        {
            PCB *newPCB = malloc(sizeof(PCB));
            memcpy(newPCB, &msg.pcb, sizeof(PCB));
            enqueueByRemainingTime(readyQueue, newPCB);
            numberOfProcesses++;
        }

        // Check preemption
        if (!isEmpty(readyQueue))
        {
            PCB *front;
            peek(readyQueue, &front);

            if (runningProcess == NULL || front->remaining_time < runningProcess->remaining_time)
            {
                if (runningProcess != NULL)
                {
                    kill(runningProcess->pid, SIGSTOP);
                    runningProcess->state = STOPPED;
                    runningProcess->stopped_time = currentTime;
                    fprintf(fptr, "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                            currentTime, runningProcess->id, runningProcess->arrival_time,
                            runningProcess->runtime, runningProcess->remaining_time,
                            runningProcess->waiting_time);
                    fflush(fptr);
                    enqueueByRemainingTime(readyQueue, runningProcess);
                }

                dequeue(readyQueue, &runningProcess);
                if (runningProcess->pid == -1)
                {
                    runningProcess->start_time = currentTime;
                    runningProcess->waiting_time = currentTime - runningProcess->arrival_time;
                    totalWaitTime += runningProcess->waiting_time;

                    int pid = fork();
                    if (pid == 0)
                    {
                        char runtime_str[5], schedulerID_str[5], id_str[5];
                        sprintf(runtime_str, "%d", runningProcess->runtime);
                        sprintf(schedulerID_str, "%d", getppid());
                        sprintf(id_str, "%d", runningProcess->id);
                        execl("./process.out", "process.out", runtime_str, schedulerID_str, id_str, NULL);
                        exit(1);
                    }
                    else
                    {
                        runningProcess->pid = pid;
                        runningProcess->state = STARTED;
                        fprintf(fptr, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                                currentTime, runningProcess->id, runningProcess->arrival_time,
                                runningProcess->runtime, runningProcess->remaining_time,
                                runningProcess->waiting_time);
                        fflush(fptr);
                    }
                }
                else
                {
                    runningProcess->state = RESUMED;
                    runningProcess->waiting_time += currentTime - runningProcess->stopped_time;
                    totalWaitTime += currentTime - runningProcess->stopped_time;
                    kill(runningProcess->pid, SIGCONT);
                    fprintf(fptr, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                            currentTime, runningProcess->id, runningProcess->arrival_time,
                            runningProcess->runtime, runningProcess->remaining_time,
                            runningProcess->waiting_time);
                    fflush(fptr);
                }
            }
        }

        if (runningProcess != NULL)
        {
            runningProcess->remaining_time--;
            totalRunTime++;
            if (runningProcess->remaining_time == 0)
            {
                runningProcess->finished_time = getClk() + 1;
                int TA = runningProcess->finished_time - runningProcess->arrival_time;
                float WTA = (float)TA / runningProcess->runtime;

                sumTA += TA;
                sumWTA += WTA;
                WTA_values[finished_index++] = WTA;

                runningProcess->state = FINISHED;
                fprintf(fptr, "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n",
                        runningProcess->finished_time, runningProcess->id,
                        runningProcess->arrival_time, runningProcess->runtime,
                        runningProcess->waiting_time, TA, WTA);
                fflush(fptr);
                runningProcess = NULL;
            }
        }
    }

    // CALCULATE PERFORMANCE METRICS
    int finishTime = getClk();
    float cpu_util = ((float)totalRunTime / finishTime) * 100.0;
    avgWaiting = (float)totalWaitTime / numberOfProcesses;
    avgWTA = sumWTA / numberOfProcesses;

    // Replace 'pow' with simple multiplication for squaring
    float sumSqDiff = 0;
    for (int i = 0; i < finished_index; ++i)
    {
        sumSqDiff += (WTA_values[i] - avgWTA) * (WTA_values[i] - avgWTA); // Squaring directly
    }

    // Now, calculate the square root without sqrt()
    float stdWTA = 0;
    float guess = sumSqDiff / numberOfProcesses; // Start with an initial guess
    float tolerance = 0.00001;                   // Set a tolerance value for convergence
    while (1)
    {
        float next_guess = 0.5 * (guess + (sumSqDiff / numberOfProcesses) / guess);
        if (abs(guess - next_guess) < tolerance)
        {
            break;
        }
        guess = next_guess;
    }
    stdWTA = guess;

    // WRITE TO PERF FILE
    FILE *perf = fopen("scheduler.perf", "w");
    if (perf)
    {
        fprintf(perf, "CPU utilization = %.2f%%\n", cpu_util);
        fprintf(perf, "Avg WTA = %.2f\n", avgWTA);
        fprintf(perf, "Avg Waiting = %.2f\n", avgWaiting);
        fprintf(perf, "Std WTA = %.2f\n", stdWTA);
        fclose(perf);
    }
    else
    {
        printf("Error opening scheduler.perf\n");
    }

    fclose(fptr);
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

    receiveNewProcessBlocking();

    // Now round robin main algorithm is here
    PCB *runningProcess = NULL;
    int quantumStartTime = 0;
    int schedulerStartingTime = 0;
    int wtaArrayIndex = 0;
    // First need to check if there is work to do
    // Process generator sets the flag queueopen = 0 to notify scheduler that is done with sending processes
    while (!isEmpty(readyQueue) || queueopen || runningProcess)
    {
        int currentTime = getClk();
        receiveNewProcessesNonBlocking(); // Receive new processes from generator

        if (runningProcess && getClk() >= quantumStartTime + quantum)
        {
            runningProcess->state = STOPPED;
            char stateString[20];
            strcpy(stateString, stateStrings[STOPPED]);
            runningProcess->stopped_time = getClk();
            fprintf(fptr, "At time %d process %d %s arr %d total %d remain %d wait %d\n", getClk(), runningProcess->id, stateString, runningProcess->arrival_time, runningProcess->runtime, runningProcess->remaining_time, runningProcess->waiting_time);
            fflush(fptr);
            enqueue(readyQueue, runningProcess);
            kill(runningProcess->pid, SIGSTOP);
            runningProcess = NULL;
        }

        if (!runningProcess && !isEmpty(readyQueue))
        {
            dequeue(readyQueue, &runningProcess);
            if (runningProcess->pid == -1)
            {
                runningProcess->pid = fork();
                if (runningProcess->pid == 0)
                {
                    char runtimeStr[15], schedulerIDStr[15], processIDStr[15];
                    sprintf(runtimeStr, "%d", runningProcess->runtime);
                    sprintf(schedulerIDStr, "%d", getppid());
                    sprintf(processIDStr, "%d", runningProcess->id);
                    execl("./process.out", "process.out", runtimeStr, schedulerIDStr, processIDStr, NULL);
                    exit(1);
                }
                runningProcess->state = STARTED;
                char stateString[10];
                strcpy(stateString, stateStrings[runningProcess->state]);
                runningProcess->start_time = getClk();
                runningProcess->waiting_time = runningProcess->start_time - runningProcess->arrival_time;
                totalRunTime += runningProcess->runtime; // Update total run time
                fprintf(fptr, "At time %d process %d %s arr %d total %d remain %d wait %d\n", currentTime, runningProcess->id, stateString, runningProcess->arrival_time, runningProcess->runtime, runningProcess->remaining_time, runningProcess->waiting_time);
                fflush(fptr);
            }
            else
            {
                runningProcess->state = RESUMED;
                char stateString[10];
                strcpy(stateString, stateStrings[runningProcess->state]);
                runningProcess->restarted_time = getClk();
                totalWaitTime -= runningProcess->waiting_time;                                                 // Removed old waiting time from total waiting time
                runningProcess->waiting_time += runningProcess->restarted_time - runningProcess->stopped_time; // Update process waiting time
                totalWaitTime += runningProcess->waiting_time;                                                 // Now add updated waiting time to total waiting time
            
                fprintf(fptr, "At time %d process %d %s arr %d total %d remain %d wait %d\n", getClk(), runningProcess->id, stateString, runningProcess->arrival_time, runningProcess->runtime, runningProcess->remaining_time, runningProcess->waiting_time);
                fflush(fptr);
                kill(runningProcess->pid, SIGCONT);
            }
            quantumStartTime = getClk(); // Set the quantum start time
        }

        if (runningProcess)
        {
            runningProcess->remaining_time--;
            if (runningProcess->remaining_time == 0)
            {
                runningProcess->state = FINISHED;
                char stateString[20];
                strcpy(stateString, stateStrings[FINISHED]);
                runningProcess->finished_time = getClk();
                int TA = runningProcess->finished_time - runningProcess->arrival_time;
                float WTA = TA / (float)runningProcess->runtime;
                WTA_values[wtaArrayIndex] = WTA;
                wtaArrayIndex++;
                sumTA += TA;
                sumWTA += WTA;
                fprintf(fptr, "At time %d process %d %s arr %d total %d remain %d wait %d TA %d WTA %.2f\n", getClk(), runningProcess->id, stateString, runningProcess->arrival_time, runningProcess->runtime, runningProcess->remaining_time, runningProcess->waiting_time, TA, WTA);
                fflush(fptr);
                runningProcess = NULL;
            }
        }

        currentTime = getClk();
        while (getClk() == currentTime);
    }

    fclose(fptr);

    int finishTime = getClk();
    printf("Finish time: %d\n", finishTime);
    printf("Total run time: %d\n", totalRunTime);
    printf("Total wait time: %d\n", totalWaitTime);
    printf("Number of processes: %d\n", numberOfProcesses);
    printf("Total turnaround time: %.2f\n", sumTA);
    printf("Total WTA: %.2f\n", sumWTA);

    float cpuUtilization = ((float)totalRunTime / (float)finishTime) * 100.0;
    float avgWaiting = (float)totalWaitTime / numberOfProcesses;
    float avgWTA = sumWTA / numberOfProcesses;
    FILE *perf;
    perf = fopen("scheduler.perf", "w");
    fprintf(perf, "CPU utilization = %.2f %% \n", cpuUtilization);
    fprintf(perf, "Avg WTA = %.2f \n", avgWTA);
    fprintf(perf, "Avg Waiting = %.2f \n", avgWaiting);
    float stdWTA = calculateStdWTA(WTA_values, wtaArrayIndex + 1, avgWTA);
    fprintf(perf, "Std WTA = %.2f \n", stdWTA);
    fclose(perf);
    freeQueue(readyQueue);
    destroyClk(true);
}

void finishedProcess(int signum)
{
    int status;
    int finishedProcessID = wait(&status); // Wait for the process to finish

    if (finishedProcessID > 0)
    {
        printf("Reaped finished process with PID: %d\n", finishedProcessID);
    }
}
void terminategenerator(int signum)
{
    queueopen = 0; // indicator that the queue has closed and no more processes will be sent from generator
}

void enqueueByRemainingTime(CircularQueue *q, PCB *pcb)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
    newNode->pcb = pcb;

    if (q->rear == NULL)
    {
        newNode->next = newNode;
        q->rear = newNode;
        return;
    }

    Node *current = q->rear->next;
    Node *prev = q->rear;
    do
    {
        if (pcb->remaining_time < current->pcb->remaining_time)
            break;
        prev = current;
        current = current->next;
    } while (current != q->rear->next);

    newNode->next = current;
    prev->next = newNode;

    // If inserted at the end, update rear
    if (prev == q->rear && pcb->remaining_time >= current->pcb->remaining_time)
    {
        q->rear = newNode;
    }
}

void peek(CircularQueue *q, PCB **pcb)
{
    if (q->rear == NULL)
    {
        *pcb = NULL;
        return;
    }
    *pcb = q->rear->next->pcb;
}

float calculateStdWTA(float WTA_values[], int count, float avgWTA)
{
    float sumSqDiff = 0.0;

    // Step 1: Calculate variance
    for (int i = 0; i < count; i++)
    {
        float diff = WTA_values[i] - avgWTA;
        sumSqDiff += diff * diff;
    }

    float variance = sumSqDiff / count;

    // Step 2: Approximate square root of variance using Babylonian method
    float guess = variance;
    float tolerance = 0.00001;

    while (1)
    {
        float next_guess = 0.5f * (guess + variance / guess);
        float diff = guess - next_guess;

        if (diff < tolerance && diff > -tolerance)
        {
            break;
        }

        guess = next_guess;
    }

    return guess;
}

void receiveNewProcessBlocking()
{
    msgbuff msg;
    int status = msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), 0, 0);
    if (status != -1)
    {
        PCB *arrivedPCB = malloc(sizeof(PCB));
        memcpy(arrivedPCB, &msg.pcb, sizeof(PCB));
        enqueue(readyQueue, arrivedPCB);
        printf("Scheduler received process %d at time %d with runtime %d and priority %d \n", arrivedPCB->id, getClk(), arrivedPCB->runtime, arrivedPCB->priority);
        numberOfProcesses++;
    }
    else
    {
        perror("Error receiving new process from message queue");
    }
}

void receiveNewProcessesNonBlocking()
{
    msgbuff msg;
    int status = msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), 1, IPC_NOWAIT);
    while (status != -1)
    {
        PCB *arrivedPCB = malloc(sizeof(PCB));
        memcpy(arrivedPCB, &msg.pcb, sizeof(PCB));
        enqueue(readyQueue, arrivedPCB);
        printf("Scheduler received process %d at time %d with runtime %d and priority %d \n", arrivedPCB->id, getClk(), arrivedPCB->runtime, arrivedPCB->priority);
        numberOfProcesses++;
        status = msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), 0, IPC_NOWAIT);
    }
}
