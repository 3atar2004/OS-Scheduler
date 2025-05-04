#include "headers.h"

/* Modify this file as needed*/
int remainingTime;
int currentTime;
int processID;

void myhandler(int signum)
{
    currentTime = getClk();
    printf("Process %d resumed at time %d\n", processID, currentTime);
}

int main(int agrc, char *argv[])
{
    initClk(); // Get global clock
    signal(SIGCONT, myhandler);
    int runningTime = atoi(argv[1]); // Gets clock as an argument from scheduler
    int schedulerID = atoi(argv[2]);
    processID = atoi(argv[3]); // Just for printing and testing flow
    currentTime = getClk();
    remainingTime = runningTime;

    // Now need to simulate the process running
    //TODO The process needs to get the remaining time from somewhere
    printf("Started running process %d\n", processID);
    while (remainingTime > 0)
    {
        if (getClk() != currentTime)
        {
            remainingTime -= getClk() - currentTime;
            currentTime = getClk();
        }
    }
    printf("Finished running process %d for %d seconds\n", processID, runningTime);
    kill(schedulerID, SIGUSR1); // send signal to scheduler to that it has finished
    destroyClk(false);

    return 0;
}
