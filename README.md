# Operating System Scheduler
![OS-logo](https://github.com/user-attachments/assets/371d60a4-0d7b-41dc-be23-98eccdc214e1)

## About
This OS scheduler simulates how a CPU manages the execution of multiple processes using fundamental operating system concepts. It recreates a realistic multi process environment using interprocess communication and scheduling algorithms.

The scheduler determines the execution order of processes based on their status and manages them using a data structure that tracks whether each process is Running, Ready, or Blocked (doing I/O, using other resources than CPU or waiting on unavailable resources). Upon creation, each process transitions through these states, closely mimicking real-world OS behavior.

Developed in C for the Linux platform, the project utilizes POSIX APIs including message queues and shared memory to coordinate process execution and synchronization of the system.

This was the final project for the CMPS303 - Operating Systems Course.

## System Architecture
The system consists of four main components. The Process Generator, the Clock, the Scheduler and Child Processes.

![image](https://github.com/user-attachments/assets/4b99829c-67b4-432a-a123-e7ef1f9401d2)

## System Breakdown
### 1. Process Generator
   - Reads process data from an input text file and creates PCB (Process Control Block) for each process read.
   - Prompts the user to select the scheduling algorithm and specify the time quantum if Round Robin is chosen.
   - Initializes the Clock and Scheduler processes, and passes the selected algorithm and quantum to the Scheduler.
   - Sends each process to the Scheduler via a message queue at its corresponding arrival time, as tracked by the Clock.

![image](https://github.com/user-attachments/assets/3de3b56e-edb5-45bb-82b2-48781d08964e)

   
### 2. Clock
   - Acts as the system’s global time reference.
   - Broadcasts the current time to all components (e.g. Process Generator and Scheduler) using shared memory.
   - Synchronizes all processes to ensure consistent execution and coordinated event logging.

![image](https://github.com/user-attachments/assets/0cb30b0f-cd87-45fe-a06f-ca18715c526d)

     
### 3. Scheduler
   - Serves as the brain of the system, making all key decisions regarding process execution.
   - Receives processes from the Process Generator via the message queue shared with the Process Generator.
   - Responsible for memory allocation and deallocation for processes using the buddy system algorithm with a total memory size of 1024 bytes.
   - Manages process execution using the chosen scheduling algorithm.
   - For each scheduled process, creates a separate Process (child process) to simulate actual process execution.
   - Controls execution using signals such as SIGCONT (to resume) and SIGSTOP (to pause), simulating CPU time allocation.
   - Tracks and switches between processes, enforcing the time quantum and mimicking preemptive multitasking.
   - After a process completes, it is properly reaped to free system resources and avoid leaving zombie processes.
   - Maintains detailed logs of process timings and memory allocation for analysis.
   - Ensures proper cleanup of all used system resources such as shared memory and message queues after the simulation ends.

     ![image](https://github.com/user-attachments/assets/aedb747c-2f9b-48ff-bf08-c8247fa02556)


### 4. Process
- Emulates a real running process created via `fork()` by the Scheduler.  
- Executes for its assigned runtime duration.
- Started, Resumed and Stopped by Scheduler using signals.  
- Sends a signal to the Scheduler upon completion to notify it for cleanup.  
- The Scheduler reaps the process to free system resources and avoid zombies.

![image](https://github.com/user-attachments/assets/454c093f-2ec9-497e-92eb-7d8ff3f16238)


## Features
### 🖥️ Interactive Command-Line Interface (CLI)
Prompts the user to:
- Select the scheduling algorithm.  
- Enter the time quantum (only required if **Round Robin** is selected).

![image](https://github.com/user-attachments/assets/222943b1-4f3f-45d6-ba0a-fd5149b8eded)


### ⚙️ Multi-Algorithm Scheduling Support
Supports the following CPU scheduling algorithms:

1. **SRTN (Shortest Remaining Time Next)**  
   Preemptive version of SJF (Shortest Job First) that always selects the process with the shortest remaining running time.

2. **HPF (Non Preemptive Highest Priority First)**  
   Schedules processes based on static priority values (lower value = higher priority).

3. **RR (Round Robin)**  
   Time sliced scheduling with a configurable quantum, simulating preemptive multitasking.
 
### 🧱 Modular System Architecture  
Clean separation between the **Process Generator**, **Scheduler**, and **Clock**, each implemented as an independent process to simulate a real OS environment.

![image](https://github.com/user-attachments/assets/7ee8e690-5735-4c2f-b0e1-69e19494c177)


### 🧪 Realistic Process Simulation  
Simulates actual process behavior by forking child processes, with execution controlled using `SIGSTOP` and `SIGCONT` signals.

![image](https://github.com/user-attachments/assets/84626566-fdec-40d0-ad86-16f58602dba3)


### 🔗 POSIX-Based IPC and Synchronization  
Employs **POSIX APIs** including message queues and shared memory for efficient and safe interprocess communication.

![image](https://github.com/user-attachments/assets/9d52f12a-ed51-4078-915c-7e1816a68ccf)


### 🧠 Memory Management Integration  
Allocates and tracks memory usage for each process using the buddy managment system. If no sufficient memory is available for a process, this process is deferred using a **Memory Waiting Priority Queue** (the lower the memory required, the higher the priority).

![image](https://github.com/user-attachments/assets/608c1152-ccc4-407c-99d3-409578faa019)


### 📝 Detailed Logging  
Logs include process execution order, state transitions, memory allocations, and statistics such as average waiting time and CPU utilization which are useful for performance analysis.

![image](https://github.com/user-attachments/assets/2f97d582-a7c5-4170-a80e-86bf01896f75)


### 🧪 Test Case Generator    
Includes a custom test generator file named **`test_generator.c`** to create edge-case and stress-test scenarios.

![image](https://github.com/user-attachments/assets/d8aa2a86-6cc8-438d-a9e0-fa897c7cc84d)


### 🧹 Safe Resource Cleanup  
Cleans up system resources after execution by detaching shared memory, removing message queues and reaping zombie processes.

![image](https://github.com/user-attachments/assets/e8e8cf2b-c1ec-4802-b8fc-15f58333b2bc)


## Collaborators
- George Ayman
- Mark Maged Nageh
- Mohamed Ahmed Mahmoud
- Omar Ahmed Reda

## Demo (Round Robin with Q = 6)



https://github.com/user-attachments/assets/7770cc5f-ebcc-4aa6-b230-f7124a76f606





## How to Install & Use

### 🔧 Requirements
- Windows with **WSL (Windows Subsystem for Linux)** enabled  
- GCC (GNU Compiler Collection)
---

### 📦 Installation

1. Clone the Repository
```
 git clone https://github.com/3atar2004/OS-Scheduler
```
2. Navigate to the project directory
```
 cd FolderName
```
### 🚀 How to Use
1. Open the project folder in your preferred IDE  
   > **Recommended:** [Visual Studio Code](https://code.visualstudio.com/) with WSL extension for smooth Linux integration

2. Open a new Ubuntu (WSL) terminal and write the following commands

3. You can view or edit the process data in `processes.txt`  
   > ⚠️ **Keep the format unchanged**
4. To generate new processes test cases using the test generator
   ```
   gcc test_generator.c -o test_generator.out
   ./test_generator.out
   ```
5. run the following command
   ```
   make run
   ```
6. Observe output in `memory.log`, `scheduler.log`, `scheduler.perf` for logging and analysis.

   







