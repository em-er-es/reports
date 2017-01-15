#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> //fork
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h> //wait
#include <semaphore.h> //semaphore
#include <fcntl.h> //semaphore

void signal_handler(int SignalNumber) {
	printf("[SIGNAL HANDLER] Process %d received signal: %d\n", getpid(), SignalNumber);
	exit(SignalNumber);
}

main()
{
pid_t Pid0, Pid1; // PID variables
signal(SIGUSR1, signal_handler); // Register signal handler
int FileDescriptor[2];
// Creating semaphores for synchronizing processes
sem_t *semaphore0 = sem_open("fathersemaphore", O_CREAT|O_EXCL, 0, 1);
sem_unlink("fathersemaphore"); // Prevent a stray semaphore
sem_t *semaphore1 = sem_open("childsemaphore", O_CREAT|O_EXCL, 0, 1);
sem_unlink("childsemaphore"); // Prevent a stray semaphore
Pid0 = getpid(); // getpid never fails
int lines = 1;
printf("[FATHER][%03d] PID: %d.\n", lines, Pid0); lines++;
int ne = 4; // Number of elements of the array to be transmitted
int InputArray[ne]; // Declaration of the array
int ResultsArray[ne];
int SpecialArray[ne];
SpecialArray[0] = 2;
SpecialArray[1] = 2;
SpecialArray[2] = 2;
SpecialArray[3] = 3;
int cycle = 1;

pipe(FileDescriptor); //Open pipe for processes
Pid1 = fork(); // Creating a child process
if (Pid1 < 0) { // Error handling
	fprintf(stderr, "[%03d][ERROR] Error forking process: %d\n", lines, Pid1); exit(1);
	}
	if (Pid1 == 0) { // Child
		sem_wait(semaphore1); // Waiting for semaphore release from father
		sem_wait(semaphore1); // Waiting for semaphore release from father
		printf("[CHILD ][%03d] PID: %d\n", lines, Pid1 = getpid()); lines++;
		while (1) {
			int ReadArray[ne];
			//~ close(FileDescriptor[1]); // Close writing end of pipe
			printf("[CHILD ][%03d] Waiting for data from father process %d\n", lines, Pid0); lines++;
			//~ while (read(FileDescriptor[0], &ReadArray, sizeof ReadArray) == 0) sleep(1); // Wait for pipe data
			sem_wait(semaphore1); // Waiting for semaphore release from father
			printf("[CHILD ][%03d] Receiving data from father process %d\n", lines, Pid0); lines++;
			int nb = read(FileDescriptor[0], &ReadArray, sizeof ReadArray); // Read pipe
			printf("[CHILD ][%03d] %d bytes received through the pipe\n", lines, nb); lines++;
			//~ close(FileDescriptor[0]); // Close reading end of pipe
			printf("[CHILD ][%03d] Received array before sorting:\n", lines); lines++;
			for (int i = 0; i < ne; i++) printf("%d ", ReadArray[i]);
			// Sort array
			int tmp;
			for (int j = ne - 1; j > 0; j--) {
				for (int i = 0; i < j; i++) {
					if (ReadArray[i] > ReadArray[i + 1]) {
						tmp = ReadArray[i];
						ReadArray[i] = ReadArray[i + 1];
						ReadArray[i + 1] = tmp;
					}
				}
			}
			printf("\n[CHILD ][%03d] Received array after sorting:\n", lines); lines++;
			for (int i = 0; i < ne; i++) printf("%d ", ReadArray[i]);
			printf("\n");
			sleep(1);
			sem_post(semaphore0); // Releasing semaphore for father
			printf("[CHILD ][%03d] Sending results to father process %d\n", lines, Pid0); lines++;
			int bw = write(FileDescriptor[1], &ReadArray, sizeof ReadArray);
			printf("[CHILD ][%03d] %d bytes written to the pipe\n[CHILD ][%03d] Data send to father process %d\n", lines, bw, lines + 1, Pid0); lines++; lines++;
			//~ close(FileDescriptor[1]); // Close writing end of pipe, father process sees EOF
		}
	} else { // Father
		while (1) {
			printf("[FATHER][%03d] Awaiting result array: %d %d %d %d\n", lines, SpecialArray[0], SpecialArray[1], SpecialArray[2], SpecialArray[3]); lines++;
			printf("[FATHER][%03d] Cycle: %d\n", lines, cycle); lines++;
			for (int i = 0; i < ne; i++) {
				printf("[FATHER][%03d] Input array [%d]:\n", lines, i); lines++;
				ResultsArray[i] = 0; // Zeroing the results array
				scanf("%d", &InputArray[i]); // Reading from user input
			}
			//~ close(FileDescriptor[0]); // Close reading end of pipe
			sem_post(semaphore1); // Releasing semaphore for child
			sleep(1);
			sem_post(semaphore1); // Releasing semaphore for child
			printf("[FATHER][%03d] Sending data to child process %d\n", lines, Pid1); lines++;
			int bw = write(FileDescriptor[1], &InputArray, sizeof InputArray);
			//~ close(FileDescriptor[1]); // Close writing end of pipe, child process sees EOF
			printf("[FATHER][%03d] %d bytes written to the pipe\n[FATHER][%03d] Data send to child process %d\n", lines, bw, lines + 1, Pid1); lines++; lines++;
			sleep(1);
			printf("[FATHER][%03d] Waiting for data from child process %d\n", lines, Pid1); lines++;
			//~ while (read(FileDescriptor[0], &ResultsArray, sizeof ResultsArray) == 0) usleep(1); // Wait for pipe data
			sem_wait(semaphore0); // Waiting for semaphore release from child
			printf("[FATHER][%03d] Awaiting results from child process %d\n", lines, Pid1); lines++;
			int nb = read(FileDescriptor[0], &ResultsArray, sizeof ResultsArray); //Read pipe
			printf("[FATHER][%03d] %d bytes received through the pipe\n", lines, nb); lines++;
			printf("[FATHER][%03d] Received results from child process:\n", lines); lines++;
			for (int i = 0; i < ne; i++) printf("%d ", ResultsArray[i]);
			printf("\n");
			int special = 0;
			for (int i = 0; i < ne; i++) { // Checking for special input
				if (ResultsArray[i] == SpecialArray[i]) special++;
			}
			if (special == ne) { // Send signal to child
				//~ kill(Pid1, SIGHUP); // man 7 signal -- Termination/hanged up signal
				// This is supposed to be the gentle way of terminating a program
				// Alternatively use termination signal 15
				//~ kill(Pid1, SIGTERM); // man 7 signal -- Termination signal
				// Using signal handler instead directly killing processes
				kill(Pid1, SIGUSR1); // man 7 signal -- Custom user signal
				// Switch between the next two lines to terminate father process
				// using a signal handler
				//~ kill(Pid0, SIGUSR1); // man 7 signal -- Custom user signal
				break; // Exiting the while loop --> father process exits
			}
			cycle++;
			printf("[FATHER][%03d] Received results different from required:\n", lines); lines++;
		}
	}
printf("[%03d]Process %d exiting\n", lines, getpid());
exit(0); // Exit for both father and child
}
