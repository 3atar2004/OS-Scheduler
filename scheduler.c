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
BuddyMemory *memory;
int main(int argc, char *argv[])
{
    initClk();
    signal(SIGUSR1, finishedProcess);
    signal(SIGUSR2, terminategenerator);
    memory = malloc(sizeof(BuddyMemory));
    memory->memsize = 1024;
    memory->start = 0;
    memory->is_free = true;
    memory->pcbID = -1;
    memory->left = NULL;
    memory->right = NULL;
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

    destroyClk(false);
    return (0);
}
void HPF()
{
    PriorityQueue *readyQueue = createQueue();
    PriorityQueue *waitingQueue = createQueue();
    PCB *current_process = NULL;
    FILE *logfile = fopen("scheduler.log", "w");
    if (!logfile)
    {
        perror("Error opening scheduler.log");
        exit(-1);
    }

    msgbuff receivedPCBbuff;
    int msgq_id;
    int totalWaitTime = 0, totalTurnaroundTime = 0, totalProcesses = 0;
    float WTA_sum = 0, runtime_sum = 0;
    key_t msg_id = ftok("msgqueue", 65);
    if (msg_id == -1)
    {
        perror("Error generating message queue key");
        fclose(logfile);
        exit(-1);
    }

    msgq_id = msgget(msg_id, 0666 | IPC_CREAT);
    if (msgq_id == -1)
    {
        perror("Error creating message queue for HPF");
        fclose(logfile);
        exit(-1);
    }

    fprintf(logfile, "#At time x process y state arr w total z remain y wait k\n");

    int clockTime = getClk();
    int done = 0; // Flag 3ashan a3raf fi processes expected tany wala la

    while (!done || !isPriEmpty(readyQueue) || current_process != NULL)
    {
        // Check for new processes fel queue
        while (msgrcv(msgq_id, &receivedPCBbuff, sizeof(receivedPCBbuff) - sizeof(long), 1, IPC_NOWAIT) != -1)
        {
            PCB *arrivedPCB = malloc(sizeof(PCB));
            if (!arrivedPCB)
            {
                perror("Error allocating memory for PCB");
                continue;
            }
            memcpy(arrivedPCB, &receivedPCBbuff.pcb, sizeof(PCB));
            arrivedPCB->waiting_time = 0;
            arrivedPCB->remaining_time = arrivedPCB->runtime;
            enqueuePri(readyQueue, arrivedPCB, arrivedPCB->priority);
            printf("Scheduler received process %d at time %d with runtime %d and priority %d\n",
                   arrivedPCB->id, getClk(), arrivedPCB->runtime, arrivedPCB->priority);
            totalProcesses++;
        }

        // Handle the currently running process
        if (current_process)
        {
            if (current_process->remaining_time <= 0)
            {
                // Process finished
                int finishTime = getClk();
                int turnaroundTime = finishTime - current_process->arrival_time;
                float wta = (float)turnaroundTime / current_process->runtime;

                fprintf(logfile, "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n",
                        finishTime, current_process->id, current_process->arrival_time, current_process->runtime,
                        current_process->waiting_time, turnaroundTime, wta);

                totalWaitTime += current_process->waiting_time;
                totalTurnaroundTime += turnaroundTime;
                WTA_sum += wta;
                runtime_sum += current_process->runtime;

                free(current_process);
                current_process = NULL;
            }
            else
            {
                // Decrement remaining time for non-preemptive execution
                current_process->remaining_time--;
            }
        }

        // Start a new process if none is running and ready queue is not empty
        if (!current_process && !isPriEmpty(readyQueue))
        {
            if (dequeuePri(readyQueue, &current_process) && current_process != NULL)
            {
                current_process->waiting_time = getClk() - current_process->arrival_time;
                fprintf(logfile, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                        getClk(), current_process->id, current_process->arrival_time, current_process->runtime,
                        current_process->remaining_time, current_process->waiting_time);
            }
        }

        int currentClk = getClk();
        if (currentClk > clockTime)
        {
            clockTime = currentClk; // Sync with clock if it advances externally
        }
        else
        {
            while (getClk() == clockTime)
            {
                usleep(1000); // Sleep briefly to reduce CPU usage
            }
            clockTime = getClk();
        }

        // Check if the message queue is still active
        struct msqid_ds buf;
        if (msgctl(msgq_id, IPC_STAT, &buf) == -1)
        {
            perror("Error checking message queue status");
            done = 1;
        }
        else if (buf.msg_qnum == 0 && totalProcesses > 0 && !current_process && isPriEmpty(readyQueue))
        {
            done = 1; // No more messages, no running process, and ready queue empty
        }
    }

    // Log performance metrics
    float cpu_utilization = (getClk() > 1) ? (runtime_sum / (getClk() - 1)) * 100 : 0;
    float avgWTA = totalProcesses > 0 ? WTA_sum / totalProcesses : 0;
    float avgWaiting = totalProcesses > 0 ? (float)totalWaitTime / totalProcesses : 0;

    FILE *perf = fopen("scheduler.perf", "w");
    if (!perf)
    {
        perror("Error opening scheduler.perf");
    }
    else
    {
        fprintf(perf, "CPU utilization = %.2f%%\n", cpu_utilization);
        fprintf(perf, "Avg WTA = %.2f\n", avgWTA);
        fprintf(perf, "Avg Waiting = %.2f\n", avgWaiting);
        fclose(perf);
    }

    fclose(logfile);
    freePriQueue(readyQueue);
    freePriQueue(waitingQueue);

    // Clean up message queue
    if (msgctl(msgq_id, IPC_RMID, NULL) == -1)
    {
        perror("Error removing message queue");
    }

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

    FILE *memoryLog = fopen("memory.log", "w");
    if (!memoryLog)
    {
        printf("Error opening memory.log\n");
        fclose(fptr);
        return;
    }
    fprintf(memoryLog, "#At time x allocated y bytes for process z from i to j\n");
    fflush(memoryLog);

    PCB *runningProcess = NULL;
    int lastTime = -1;

    while (!isEmpty(readyQueue) || queueopen || runningProcess != NULL)
    {
        int currentTime = getClk();
        if (currentTime == lastTime)
            continue;
        lastTime = currentTime;

        // Receive new arrivals
        receiveNewProcessesNonBlocking();

        // Check for preemption
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
                    // Allocate memory for the process
                    if (!allocatememory(memory, runningProcess))
                    {
                        printf("Memory allocation failed for process %d\n", runningProcess->id);
                        enqueueByRemainingTime(readyQueue, runningProcess);
                        runningProcess = NULL;
                        continue;
                    }

                    fprintf(memoryLog, "At time %d allocated %d bytes for process %d from %d to %d\n",
                            currentTime, runningProcess->memorysize, runningProcess->id,
                            runningProcess->startaddress, runningProcess->endaddress);
                    fflush(memoryLog);

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
                runningProcess->finished_time = getClk();
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

                // Free memory for the process
                deallocatememory(memory, runningProcess->startaddress);
                fprintf(memoryLog, "At time %d freed %d bytes from process %d from %d to %d\n",
                        runningProcess->finished_time, runningProcess->memorysize, runningProcess->id,
                        runningProcess->startaddress, runningProcess->endaddress);
                fflush(memoryLog);

                free(runningProcess);
                runningProcess = NULL;
            }
        }
    }

    // CALCULATE PERFORMANCE METRICS
    int finishTime = getClk();
    float cpu_util = ((float)totalRunTime / finishTime) * 100.0;
    avgWaiting = (float)totalWaitTime / numberOfProcesses;
    avgWTA = sumWTA / numberOfProcesses;

    // Calculate standard deviation of WTA using the provided function
    stdWTA = calculateStdWTA(WTA_values, numberOfProcesses, avgWTA);

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
    fclose(memoryLog);
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
    receiveNewProcessesNonBlocking();
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
                fprintf(fptr, "At time %d process %d %s arr %d total %d remain %d wait %d TA %d WTA %.2f\n", getClk() + 1, runningProcess->id, stateString, runningProcess->arrival_time, runningProcess->runtime, runningProcess->remaining_time, runningProcess->waiting_time, TA, WTA);
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
