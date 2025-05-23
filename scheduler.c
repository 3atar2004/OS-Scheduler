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
int process_finished_flag = 0;
int finished_process_pid=-1;

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
PriorityQueue *waitingQueue;
FILE *fptr;
FILE *mptr;
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

    waitingQueue = createQueue();


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
void HPF() {
    PriorityQueue *readyqueue = createQueue();
    PriorityQueue *waitingQueue = createQueue(); 
    PCB *current_process = NULL;
    FILE *logfile = fopen("scheduler.log", "w");
    if (!logfile) {
        perror("Error opening scheduler.log");
        exit(-1);
    }
    FILE *memfile = fopen("memory.log", "w");
    if (!memfile) {
        perror("Error opening memory.log");
        exit(-1);
    }

    msgbuff receivedPCBbuff;
    int totalWaitTime = 0, totalTurnaroundTime = 0, totalProcesses = 0;
    float WTA_sum = 0, runtime_sum = 0;

    fprintf(logfile, "#At time x process y state arr w total z remain y wait k\n");

    int clockTime = getClk();
    int done = 0;

    // Receive initial processes
    while (msgrcv(msgq_id, &receivedPCBbuff, sizeof(receivedPCBbuff) - sizeof(long), 1, IPC_NOWAIT) != -1) {
        printf("Received initial process\n");
        PCB *arrivedPCB = malloc(sizeof(PCB));
        if (!arrivedPCB) {
            perror("Error allocating memory for PCB");
            continue;
        }
        memcpy(arrivedPCB, &receivedPCBbuff.pcb, sizeof(PCB));
        arrivedPCB->waiting_time = 0;
        arrivedPCB->remaining_time = arrivedPCB->runtime;
        arrivedPCB->pid = -1;
        // Add to waiting queue to respect arrival time
        enqueuePri(waitingQueue, arrivedPCB, arrivedPCB->priority);
        printf("Scheduler received process %d at time %d with runtime %d and priority %d\n",
               arrivedPCB->id, getClk(), arrivedPCB->runtime, arrivedPCB->priority);
        totalProcesses++;
    }

    while (!done || !isPriEmpty(readyqueue) || !isPriEmpty(waitingQueue) || current_process != NULL) {
        int currentTime = getClk();

        // Check for new processes
        while (msgrcv(msgq_id, &receivedPCBbuff, sizeof(receivedPCBbuff) - sizeof(long), 1, IPC_NOWAIT) != -1) {
            PCB *arrivedPCB = malloc(sizeof(PCB));
            if (!arrivedPCB) {
                perror("Error allocating memory for PCB");
                continue;
            }
            memcpy(arrivedPCB, &receivedPCBbuff.pcb, sizeof(PCB));
            arrivedPCB->waiting_time = 0;
            arrivedPCB->remaining_time = arrivedPCB->runtime;
            arrivedPCB->pid = -1;
            enqueuePri(waitingQueue, arrivedPCB, arrivedPCB->priority);
            printf("Scheduler received process %d at time %d with runtime %d and priority %d\n",
                   arrivedPCB->id, getClk(), arrivedPCB->runtime, arrivedPCB->priority);
            totalProcesses++;
        }

        // Move processes from waiting queue to ready queue based on arrival time
        while (!isPriEmpty(waitingQueue)) {
            PCB *waitingProcess;
            if (dequeuePri(waitingQueue, &waitingProcess)) {
                if (waitingProcess->arrival_time <= currentTime) {
                    enqueuePri(readyqueue, waitingProcess, waitingProcess->priority);
                    printf("Process %d moved to ready queue at time %d\n", waitingProcess->id, currentTime);
                } else {
                    enqueuePri(waitingQueue, waitingProcess, waitingProcess->priority);
                    break;
                }
            }
        }

        // Check if current process has finished
        if (current_process) {
            printf("Checking if process %d (pid=%d) finished at time %d\n", current_process->id, current_process->pid, currentTime);
            // Check via signal handler flag
            if (process_finished_flag && finished_process_pid == current_process->pid) {
                printf("Process %d finished via signal at time %d\n", current_process->id, currentTime);
                int finishTime = getClk();
                int turnaroundTime = finishTime - current_process->arrival_time;
                float wta = (float)turnaroundTime / current_process->runtime;

                deallocatememory(memory, current_process->startaddress);
                fprintf(memfile, "At time %d freed %d bytes from process %d from %d to %d\n",
                        getClk(), current_process->memorysize, current_process->id,
                        current_process->startaddress, current_process->endaddress);
                fflush(memfile);

                fprintf(logfile, "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n",
                        finishTime, current_process->id, current_process->arrival_time, current_process->runtime,
                        current_process->waiting_time, turnaroundTime, wta);

                totalTurnaroundTime += turnaroundTime;
                WTA_sum += wta;
                runtime_sum += current_process->runtime;

                free(current_process);
                current_process = NULL;
                process_finished_flag = 0;
                finished_process_pid = -1;
            }
        }

        // Start a new process if none is running and ready queue is not empty
        if (!current_process && !isPriEmpty(readyqueue)) {
            if (dequeuePri(readyqueue, &current_process) && current_process != NULL) {
                current_process->waiting_time = getClk() - current_process->arrival_time;
                totalWaitTime += current_process->waiting_time;
                allocatememory(memory, current_process);

                int pid = fork();
                if (pid == -1) {
                    perror("Fork failed");
                    free(current_process);
                    current_process = NULL;
                    continue;
                }
                if (pid == 0) {
                    char runtime_str[10], schedulerID_str[10], id_str[10];
                    sprintf(runtime_str, "%d", current_process->runtime);
                    sprintf(schedulerID_str, "%d", getppid());
                    sprintf(id_str, "%d", current_process->id);
                    execl("./process.out", "process.out", runtime_str, schedulerID_str, id_str, NULL);
                    perror("execl failed");
                    exit(1);
                } else {

                    fprintf(memfile, "At time %d allocated %d bytes for process %d from %d to %d\n",
                            currentTime, current_process->memorysize, current_process->id,
                            current_process->startaddress, current_process->endaddress);
                    fflush(memfile);
                    current_process->pid = pid;
                    fprintf(logfile, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                            getClk(), current_process->id, current_process->arrival_time, current_process->runtime,
                            current_process->remaining_time, current_process->waiting_time);
                    fflush(logfile);
                    printf("Started process %d (pid=%d) at time %d\n", current_process->id, pid, getClk());
                }
            }
        }

        //Check termination condition
        struct msqid_ds buf;
        if (msgctl(msgq_id, IPC_STAT, &buf) == -1) {
            perror("Error checking message queue status");
            done = 1;
        } else if (buf.msg_qnum == 0 && totalProcesses > 0 && !current_process && 
                   isPriEmpty(readyqueue) && isPriEmpty(waitingQueue)) {
            done = 1;
        }

        // Clock synchronization
        while (getClk() == clockTime) {
            usleep(1000);
        }
        clockTime = getClk();
    }

    // Log performance metrics
    float cpu_utilization = (clockTime > 1) ? (runtime_sum / (clockTime - 1)) * 100 : 0;
    float avgWTA = totalProcesses > 0 ? WTA_sum / totalProcesses : 0;
    float avgWaiting = totalProcesses > 0 ? (float)totalWaitTime / totalProcesses : 0;

    FILE *perf = fopen("scheduler.perf", "w");
    if (!perf) {
        perror("Error opening scheduler.perf");
    } else {
        fprintf(perf, "CPU utilization = %.2f%%\n", cpu_utilization);
        fprintf(perf, "Avg WTA = %.2f\n", avgWTA);
        fprintf(perf, "Avg Waiting = %.2f\n", avgWaiting);
        fclose(perf);
    }

    fclose(logfile);
    freePriQueue(readyqueue);
    freePriQueue(waitingQueue);

    if (msgctl(msgq_id, IPC_RMID, NULL) == -1) {
        perror("Error removing message queue");
    }
}

void SRTN()
{
    fptr = fopen("scheduler.log", "w");
    if (!fptr)
    {
        printf("Error opening scheduler.log\n");
        return;
    }
    mptr = fopen("memory.log", "w");
    if (!mptr)
    {
        printf("Error opening memory.log\n");
        fclose(fptr);
        return;
    }

    fprintf(fptr, "#At time x process y state arr w total z remain y wait k\n");
    fflush(fptr);
    fprintf(mptr, "#At time x allocated y bytes for process z from i to j\n");
    fflush(mptr);

    PCB *runningProcess = NULL;
    int wtaArrayIndex = 0;

    // FIX: Receive the first process before entering the loop
    receiveNewProcessBlocking();

    while (!isEmpty(readyQueue) || queueopen || runningProcess)
    {
        int currentTime = getClk();
        receiveNewProcessesNonBlocking();

        // Try to allocate memory for waiting processes after any change
        while (!isPriEmpty(waitingQueue))
        {
            PCB *waitingProcess;
            peekPri(waitingQueue, &waitingProcess);
            if (allocatememory(memory, waitingProcess))
            {
                dequeuePri(waitingQueue, &waitingProcess);
                enqueueByRemainingTime(readyQueue, waitingProcess);
                fprintf(mptr, "At time %d allocated %d bytes for process %d from %d to %d\n",
                        getClk(), waitingProcess->memorysize, waitingProcess->id,
                        waitingProcess->startaddress, waitingProcess->endaddress);
                fflush(mptr);
            }
            else break;
        }

        // Preemption: always run the process with the shortest remaining time
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
                runningProcess->start_time = getClk();
                runningProcess->waiting_time = runningProcess->start_time - runningProcess->arrival_time;
                totalWaitTime += runningProcess->waiting_time;
                fprintf(fptr, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                        currentTime, runningProcess->id, runningProcess->arrival_time,
                        runningProcess->runtime, runningProcess->remaining_time, runningProcess->waiting_time);
                fflush(fptr);
            }
            else
            {
                runningProcess->state = RESUMED;
                runningProcess->restarted_time = getClk();
                runningProcess->waiting_time += runningProcess->restarted_time - runningProcess->stopped_time;
                totalWaitTime += runningProcess->restarted_time - runningProcess->stopped_time;
                fprintf(fptr, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                        getClk(), runningProcess->id, runningProcess->arrival_time,
                        runningProcess->runtime, runningProcess->remaining_time, runningProcess->waiting_time);
                fflush(fptr);
                kill(runningProcess->pid, SIGCONT);
            }
        }

        // Preempt if a new process with shorter remaining time arrives
        if (runningProcess && !isEmpty(readyQueue))
        {
            PCB *front;
            peek(readyQueue, &front);
            if (front->remaining_time < runningProcess->remaining_time)
            {
                runningProcess->state = STOPPED;
                runningProcess->stopped_time = getClk();
                fprintf(fptr, "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                        getClk(), runningProcess->id, runningProcess->arrival_time,
                        runningProcess->runtime, runningProcess->remaining_time, runningProcess->waiting_time);
                fflush(fptr);
                enqueueByRemainingTime(readyQueue, runningProcess);
                kill(runningProcess->pid, SIGSTOP);
                runningProcess = NULL;
                continue;
            }
        }

        // Run the current process for one tick
        if (runningProcess)
        {
            runningProcess->remaining_time--;
            totalRunTime++;
            if (runningProcess->remaining_time == 0)
            {
                runningProcess->state = FINISHED;
                runningProcess->finished_time = getClk();
                int TA = runningProcess->finished_time - runningProcess->arrival_time;
                float WTA = (float)TA / runningProcess->runtime;
                WTA_values[wtaArrayIndex++] = WTA;
                sumTA += TA;
                sumWTA += WTA;
                fprintf(fptr, "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n",
                        getClk(), runningProcess->id, runningProcess->arrival_time,
                        runningProcess->runtime, runningProcess->waiting_time, TA, WTA);
                fflush(fptr);

                deallocatememory(memory, runningProcess->startaddress);
                fprintf(mptr, "At time %d freed %d bytes from process %d from %d to %d\n",
                        getClk(), runningProcess->memorysize, runningProcess->id,
                        runningProcess->startaddress, runningProcess->endaddress);
                fflush(mptr);

                free(runningProcess);
                runningProcess = NULL;
            }
        }

        // Busy wait for the next clock tick
        int tick = getClk();
        while (getClk() == tick);

        // FIX: Terminate if all queues are empty and generator is done
        if (isEmpty(readyQueue) && !runningProcess && !queueopen && isPriEmpty(waitingQueue))
            break;
    }

    fclose(fptr);
    fclose(mptr);

    int finishTime = getClk();
    float cpuUtilization = ((float)totalRunTime / (float)finishTime) * 100.0;
    float avgWaiting = (float)totalWaitTime / numberOfProcesses;
    float avgWTA = sumWTA / numberOfProcesses;
    float stdWTA = calculateStdWTA(WTA_values, wtaArrayIndex, avgWTA);
    FILE *perf = fopen("scheduler.perf", "w");
    fprintf(perf, "CPU utilization = %.2f %% \n", cpuUtilization);
    fprintf(perf, "Avg WTA = %.2f \n", avgWTA);
    fprintf(perf, "Avg Waiting = %.2f \n", avgWaiting);
    fprintf(perf, "Std WTA = %.2f \n", stdWTA);
    fclose(perf);
    freeQueue(readyQueue);
}

void RR(int quantum)
{
    fptr = fopen("scheduler.log", "w");
    if (!fptr)
    {
        printf("Error opening scheduler.log\n");
        return;
    }
    mptr = fopen("memory.log", "w");
    if (!mptr)
    {
        printf("Error opening memory.log\n");
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
                // fprintf(mptr, "At time %d allocated %d bytes for process %d from %d to %d\n", currentTime, runningProcess->memorysize, runningProcess->id, runningProcess->startaddress, runningProcess->endaddress);
                // fflush(mptr);
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
                deallocatememory(memory, runningProcess->startaddress);
                fprintf(mptr, "At time %d freed %d bytes from process %d from %d to %d\n", getClk() + 1, runningProcess->memorysize, runningProcess->id, runningProcess->startaddress, runningProcess->endaddress);
                fflush(mptr);
                runningProcess = NULL;
                while (!isPriEmpty(waitingQueue))
                {
                    PCB *waitingProcess;
                    peekPri(waitingQueue, &waitingProcess);
                    if (allocatememory(memory, waitingProcess))
                    {
                        dequeuePri(waitingQueue, &waitingProcess);
                        printf("At time %d Dequeued process %d from waiting queue\n", getClk() + 1, waitingProcess->id);
                        enqueue(readyQueue, waitingProcess);
                        printf("At time %d Enqueued process %d to ready queue\n", getClk() + 1, waitingProcess->id);
                        fprintf(mptr, "At time %d allocated %d bytes for process %d from %d to %d\n", getClk() + 1, waitingProcess->memorysize, waitingProcess->id, waitingProcess->startaddress, waitingProcess->endaddress);
                        fflush(mptr);
                    }
                    else break;
                }
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
    int finished_pid = wait(&status);
    if (finished_pid > 0) {
        printf("Reaped finished process with PID: %d\n", finished_pid);
        process_finished_flag = 1;
        finished_process_pid = finished_pid;
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
        if (allocatememory(memory, arrivedPCB) == -1)
        {
            printf("Memory allocation failed for process %d, enqueing in waiting queue!\n", arrivedPCB->id);
            enqueuePri(waitingQueue, arrivedPCB, arrivedPCB->memorysize);
            return;
        }
        printf("Successful memory allocation for process %d, enqueing in ready queue!\n", arrivedPCB->id);
        enqueue(readyQueue, arrivedPCB);
        printf("Allocated %d bytes for process %d from %d to %d\n", arrivedPCB->memorysize, arrivedPCB->id, arrivedPCB->startaddress, arrivedPCB->endaddress);
        fprintf(mptr, "At time %d allocated %d bytes for process %d from %d to %d\n", getClk(), arrivedPCB->memorysize, arrivedPCB->id, arrivedPCB->startaddress, arrivedPCB->endaddress);
        fflush(mptr);
        printf("Scheduler received process %d at time %d with runtime %d and priority %d \n", arrivedPCB->id, getClk(), arrivedPCB->runtime, arrivedPCB->priority);
        numberOfProcesses++;
    }
    else
    {
        perror("Error receiving new process from message queue!\n");
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
        if (!allocatememory(memory, arrivedPCB))
        {
            printf("Memory allocation failed for process %d, enqueing in waiting queue!\n", arrivedPCB->id);
            enqueuePri(waitingQueue, arrivedPCB, arrivedPCB->memorysize);
        }
        else
        {
            enqueue(readyQueue, arrivedPCB);
            printf("Successful memory allocation for process %d, enqueing in ready queue!\n", arrivedPCB->id);
            printf("Allocated %d bytes for process %d from %d to %d\n", arrivedPCB->memorysize, arrivedPCB->id, arrivedPCB->startaddress, arrivedPCB->endaddress);
            fprintf(mptr, "At time %d allocated %d bytes for process %d from %d to %d\n", getClk(), arrivedPCB->memorysize, arrivedPCB->id, arrivedPCB->startaddress, arrivedPCB->endaddress);
            fflush(mptr);
            printf("Scheduler received process %d at time %d with runtime %d and priority %d \n", arrivedPCB->id, getClk(), arrivedPCB->runtime, arrivedPCB->priority);
            numberOfProcesses++;
        }
        status = msgrcv(msgq_id, &msg, sizeof(msg) - sizeof(long), 0, IPC_NOWAIT);
    }
}
