#include <stdio.h>
#include <stdlib.h>
#include "headers.h" // Include the BuddyMemory and PCB definitions

int main()
{
    // Initialize root memory block
    BuddyMemory *root = malloc(sizeof(BuddyMemory));
    root->memsize = 1024;
    root->start = 0;
    root->is_free = true;
    root->pcbID = -1;
    root->left = root->right = NULL;

    // Define PCBs with 256 memory size
    PCB pcb1 = {
        .id = 1,
        .arrival_time = 4,
        .runtime = 11,
        .priority = 9,
        .state = 0,
        .remaining_time = 11,
        .waiting_time = 0,
        .pid = -1,
        .start_time = -1,
        .finished_time = -1,
        .stopped_time = -1,
        .restarted_time = -1,
        .remainingTimeAfterStop = -1,
        .memorysize = 256,
        .startaddress = -1,
        .endaddress = -1};

    PCB pcb2 = {
        .id = 2,
        .arrival_time = 7,
        .runtime = 2,
        .priority = 8,
        .state = 0,
        .remaining_time = 2,
        .waiting_time = 0,
        .pid = -1,
        .start_time = -1,
        .finished_time = -1,
        .stopped_time = -1,
        .restarted_time = -1,
        .remainingTimeAfterStop = -1,
        .memorysize = 256,
        .startaddress = -1,
        .endaddress = -1};

    PCB pcb3 = {
        .id = 3,
        .arrival_time = 8,
        .runtime = 28,
        .priority = 0,
        .state = 0,
        .remaining_time = 28,
        .waiting_time = 0,
        .pid = -1,
        .start_time = -1,
        .finished_time = -1,
        .stopped_time = -1,
        .restarted_time = -1,
        .remainingTimeAfterStop = -1,
        .memorysize = 256,
        .startaddress = -1,
        .endaddress = -1};

    PCB pcb4 = {
        .id = 4,
        .arrival_time = 13,
        .runtime = 7,
        .priority = 6,
        .state = 0,
        .remaining_time = 7,
        .waiting_time = 0,
        .pid = -1,
        .start_time = -1,
        .finished_time = -1,
        .stopped_time = -1,
        .restarted_time = -1,
        .remainingTimeAfterStop = -1,
        .memorysize = 256,
        .startaddress = -1,
        .endaddress = -1};

    PCB pcb5 = {
        .id = 5,
        .arrival_time = 22,
        .runtime = 7,
        .priority = 8,
        .state = 0,
        .remaining_time = 7,
        .waiting_time = 0,
        .pid = -1,
        .start_time = -1,
        .finished_time = -1,
        .stopped_time = -1,
        .restarted_time = -1,
        .remainingTimeAfterStop = -1,
        .memorysize = 256,
        .startaddress = -1,
        .endaddress = -1};

    PCB pcb6 = {
        .id = 6,
        .arrival_time = 25,
        .runtime = 5,
        .priority = 7,
        .state = 0,
        .remaining_time = 5,
        .waiting_time = 0,
        .pid = -1,
        .start_time = -1,
        .finished_time = -1,
        .stopped_time = -1,
        .restarted_time = -1,
        .remainingTimeAfterStop = -1,
        .memorysize = 256,
        .startaddress = -1,
        .endaddress = -1};

    // Allocate 4 PCBs
    printf("Allocating PCB 1 (256 bytes): %s", allocatememory(root, &pcb1) ? "Success" : "Failed");
    printf(" in start = %d\n", pcb1.startaddress);
    printf("Allocating PCB 2 (256 bytes): %s", allocatememory(root, &pcb2) ? "Success" : "Failed");
    printf(" in start = %d\n", pcb2.startaddress);
    printf("Allocating PCB 3 (256 bytes): %s", allocatememory(root, &pcb3) ? "Success" : "Failed");
    printf(" in start = %d\n", pcb3.startaddress);
    printf("Allocating PCB 4 (256 bytes): %s", allocatememory(root, &pcb4) ? "Success" : "Failed");
    printf(" in start = %d\n", pcb4.startaddress);

    // Display memory tree
    printf("\nMemory Tree After Allocations:\n");
    displayTree(root, 0, "Root");

    // Deallocate PCB 1
    printf("\nDeallocating PCB 1:\n");
    deallocatememory(root, pcb1.startaddress);

    // Display memory tree after deallocation
    printf("\nMemory Tree After Deallocating PCB 1:\n");
    displayTree(root, 0, "Root");

    // Allocate 2 more PCBs
    printf("\nAllocating PCB 5 (256 bytes): %s", allocatememory(root, &pcb5) ? "Success" : "Failed");
    printf(" in start = %d\n", pcb5.startaddress);
    printf("Allocating PCB 6 (256 bytes): %s", allocatememory(root, &pcb6) ? "Success" : "Failed");
    printf(" in start = %d\n", pcb6.startaddress);

    // Display final memory tree
    printf("\nFinal Memory Tree:\n");
    displayTree(root, 0, "Root");

    // Free the root block
    free(root);
    return 0;
}