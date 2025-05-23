#include <stdio.h> //if you don't use scanf/printf change this include
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

typedef short bool;
#define true 1
#define false 0

#define SHKEY 300
typedef struct BuddyMemory
{
    int memsize;
    int start;
    bool is_free;
    int pcbID;
    struct BuddyMemory *left;
    struct BuddyMemory *right;
} BuddyMemory;
typedef struct PCB
{
    int id; // read from file
    int arrival_time;
    int runtime;
    int priority;
    int state;
    int remaining_time;
    int waiting_time;
    int pid; // forking id
    int start_time;
    int finished_time;
    int stopped_time;
    int restarted_time;
    int remainingTimeAfterStop;
    int memorysize;
    int startaddress;
    int endaddress;
} PCB;

enum STATE
{
    READY,    // 0
    RUNNING,  // 1
    FINISHED, // 2
    STARTED,  // 3
    RESUMED,  // 4
    STOPPED   // 5
};

const char *stateStrings[] = {"ready", "running", "finished", "started", "resumed", "stopped"};

typedef struct msgbuff
{
    long mtype;
    PCB pcb;
} msgbuff;

///==============================
// don't mess with this variable//
int *shmaddr; //
//===============================

int getClk()
{
    return *shmaddr;
}

/*
 * All processes call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
 */
void initClk()
{
    int shmid = shmget(SHKEY, 4, 0444);
    while ((int)shmid == -1)
    {
        // Make sure that the clock exists
        printf("Wait! The clock not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
    }
    shmaddr = (int *)shmat(shmid, (void *)0, 0);
}

/*
 * All processes call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
 */

void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}

// Node structure for the queue
typedef struct Node
{
    PCB *pcb;
    struct Node *next;
} Node;

// Circular queue structure
typedef struct
{
    Node *rear;
} CircularQueue;

// Initialize the circular queue
void initQueue(CircularQueue *q)
{
    q->rear = NULL;
}

// Check if the queue is empty
bool isEmpty(CircularQueue *q)
{
    return q->rear == NULL;
}

// Enqueue an element into the queue
void enqueue(CircularQueue *q, PCB *pcb)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (!newNode)
    {
        printf("Memory allocation failed\n");
        return;
    }
    if (pcb != NULL)
    {
        newNode->pcb = pcb;

        if (isEmpty(q))
        {
            newNode->next = newNode; // Points to itself
            q->rear = newNode;
        }
        else
        {
            newNode->next = q->rear->next;
            q->rear->next = newNode;
            q->rear = newNode;
        }
    }
    else
    {
        printf("PCB is NULL \n");
        free(newNode);
        return;
    }
}

// Dequeue an element from the queue
bool dequeue(CircularQueue *q, PCB **retpcb)
{
    if (isEmpty(q))
    {
        *retpcb = NULL;
        return false; // Queue is empty
    }

    Node *temp = q->rear->next;
    *retpcb = temp->pcb;

    if (q->rear == temp)
    {
        q->rear = NULL; // Queue is now empty
    }
    else
    {
        q->rear->next = temp->next;
    }

    free(temp);
    return true;
}

void peak(CircularQueue *q, PCB **retpcb)
{
    if (isEmpty(q))
    {
        *retpcb = NULL;
        return;
    }
    Node *temp = q->rear->next;
    *retpcb = temp->pcb;
}


// Display the elements of the queue
// void displayQueue(CircularQueue* q) {
//     if (isEmpty(q)) {
//         printf("Queue is empty\n");
//         return;
//     }

//     Node* temp = q->rear->next;
//     printf("Queue elements: ");
//     do {
//         printf("%d ", temp->pcb->pri);
//         temp = temp->next;
//     } while (temp != q->rear->next);
//     printf("\n");
// }

// Free all nodes in the queue
void freeQueue(CircularQueue *q)
{
    if (isEmpty(q))
    {
        return;
    }

    Node *current = q->rear->next;
    Node *nextNode;
    while (current != q->rear)
    {
        nextNode = current->next;
        free(current);
        current = nextNode;
    }
    free(q->rear);
    q->rear = NULL;
}

typedef struct priNode
{
    PCB *pcb;
    struct priNode *next;
    int priority;
} priNode;

typedef struct PriorityQueue
{
    priNode *front;
} PriorityQueue;

int peekPri(PriorityQueue *pq, PCB **pcb)
{
    if (pq == NULL || pq->front == NULL)
        return 0;

    *pcb = pq->front->pcb;
    return 1;
}
// Create a new priority queue
PriorityQueue *createQueue()
{
    PriorityQueue *pq = (PriorityQueue *)malloc(sizeof(PriorityQueue));
    pq->front = NULL;
    return pq;
}

// Check if the queue is empty
bool isPriEmpty(PriorityQueue *pq)
{
    return pq->front == NULL;
}

// Enqueue an element in ascending order
void enqueuePri(PriorityQueue *pq, PCB *pcb, int pri)
{
    priNode *newNode = (priNode *)malloc(sizeof(priNode));
    newNode->pcb = pcb;
    newNode->next = NULL;
    newNode->priority = pri;

    // If the queue is empty or the new node has the highest priority (smallest value)
    if (pq->front == NULL || pq->front->priority > pri)
    {
        newNode->next = pq->front;
        pq->front = newNode;
    }
    else
    {
        // Find the correct position to insert the new node
        priNode *current = pq->front;
        while (current->next != NULL && current->next->priority <= pri)
        {
            current = current->next;
        }
        newNode->next = current->next;
        current->next = newNode;
    }
}

// Dequeue the element with the highest priority (smallest value)
bool dequeuePri(PriorityQueue *pq, PCB **retpcb)
{
    if (isPriEmpty(pq))
    {
        printf("Queue is empty!\n");
        return false;
    }
    priNode *temp = pq->front;

    // Return the pointer to the dequeued PCB
    *retpcb = temp->pcb;

    pq->front = pq->front->next;
    free(temp);
    return true;
}

// Free the memory allocated for the queue
void freePriQueue(PriorityQueue *pq)
{
    while (!isPriEmpty(pq))
    {
        PCB *tempPcb = NULL;
        dequeuePri(pq, &tempPcb);
        // PCB memory is not freed here because it should be managed by the caller
    }
    free(pq);
}

// void printQueue(CircularQueue *q)
// {
//     if(isEmpty(q))
//     {
//         printf("Queue is empty\n");
//         return;
//     }
//     Node *current = q->rear->next;
//     do
//     {
//         printf("ID: %d, Arrival: %d, Runtime: %d, Priority: %d\n", current->pcb.id, current->pcb.arrival_time, current->pcb.runtime, current->pcb.priority);
//         current = current->next;
//     } while(current != q->rear->next);
// }

// memory management part
#define TotalMemorySize 1024
#define minMemorySize 1
#define maxMemorySize 512

BuddyMemory * createNode(int start,int size)
{
    BuddyMemory * node=(BuddyMemory *)malloc(sizeof(BuddyMemory));
    node->memsize=size;
    node->start=start;
    node->is_free=true;
    node->pcbID=-1;
    node->left=node->right=NULL;
    return node;
}
BuddyMemory* initialise()
{
    return createNode(0,TotalMemorySize);
}
int getnextpowerof2(int x)
{
    int power=1;
    while(power<x)
    {
        power*=2;
    }  
    return power;
}
bool allocatememory(BuddyMemory *node, PCB *pcb)
{
    if (!node || !pcb)
        return false;

    int requestedsize = pcb->memorysize;
    int requiredBlockSize = getnextpowerof2(requestedsize);

    // Can't allocate if already taken or block is too small
    if (!node->is_free || node->memsize < requiredBlockSize)
        return false;

    // Leaf node and exactly fits — allocate
    if (node->memsize == requiredBlockSize && node->left == NULL && node->right == NULL)
    {
        node->is_free = false;
        node->pcbID = pcb->id;
        pcb->startaddress = node->start;
        pcb->endaddress = node->start + node->memsize - 1;
        return true;
    }

    // Split if not already split
    if (node->left == NULL && node->right == NULL)
    {
        int halfSize = node->memsize / 2;
        if (halfSize < minMemorySize)
            return false; // can't split further

        node->left = createNode(node->start, halfSize);
        node->right = createNode(node->start + halfSize, halfSize);
    }

    // Recurse: try left first
    if (allocatememory(node->left, pcb))
        return true;

    // Then try right
    if (allocatememory(node->right, pcb))
        return true;

    return false;
}

int deallocatememory(BuddyMemory *node, int startaddress)
{
    if (!node) return 0;

    // Base case: leaf node
    if (node->left == NULL && node->right == NULL)
    {
        if (node->start == startaddress && !node->is_free)
        {
            node->is_free = true;
            node->pcbID = -1;
            return 1;
        }
        return 0;
    }

    // Recurse into children
    int leftfree = deallocatememory(node->left, startaddress);
    int rightfree = deallocatememory(node->right, startaddress);

    // Merge only if a block was freed and both children are free, unallocated, and leaf nodes
    if ((leftfree || rightfree) &&
        node->left && node->right &&
        node->left->is_free && node->right->is_free &&
        node->left->pcbID == -1 && node->right->pcbID == -1 &&
        node->left->left == NULL && node->left->right == NULL &&
        node->right->left == NULL && node->right->right == NULL)
    {
        free(node->left);
        free(node->right);
        node->left = NULL;
        node->right = NULL;
        node->is_free = true;
        node->pcbID = -1;
        return 1;
    }

    return leftfree || rightfree;
}
void displayTree(BuddyMemory *head, int depth, const char *position)
{
    if (!head)
    {
        return;
    }

    // Indent based on depth to visualize tree levels
    for (int i = 0; i < depth; i++)
    {
        printf("  ");
    }

    // Display whether the block is "Left" or "Right" or "Root"
    if (position)
    {
        printf("[%s] ", position);
    }

    // Print information about the block
    printf("Block: Start=%d, Size=%d, Free=%s",
           head->start, head->memsize, head->is_free ? "Yes" : "No");

    // If allocated, display the PCB ID and memory range
    if (!head->is_free && head->pcbID != -1)
    {
        printf(", Allocated to PCB ID=%d (Start=%d, End=%d)",
               head->pcbID, head->start, head->start + head->memsize - 1);
    }

    printf("\n");

    // Recursively display left and right children
    displayTree(head->left, depth + 1, "Left");
    displayTree(head->right, depth + 1, "Right");
}