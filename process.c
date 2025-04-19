#include "headers.h"

/* Modify this file as needed*/
int remainingtime;
int time;
void myhandler(int signum)
{
    time=getClk();
}

int main(int agrc, char *argv[])
{
    initClk();
    signal(SIGCONT, myhandler);
    remainingtime=0;
    time=getClk();
    //TODO The process needs to get the remaining time from somewhere

    while (remainingtime > 0)
    {
        if(getClk()!=time)
        {
            remainingtime-=getClk()-time;
            time=getClk();
        }
    }
    kill(getppid(), SIGUSR1); // send signal to scheduler to that it has finished
    destroyClk(false);

    return 0;
}
