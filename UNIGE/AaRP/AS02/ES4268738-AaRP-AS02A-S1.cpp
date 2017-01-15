/*
Assignment 02 - Advanced and Robot Programming
Publisher, subscriber and mediator scheme in POSIX environment
Ernest Skrzypczyk -- 4268738 -- EMARO+ // ES4268738-AaRP-AS02
*/
// Subscriber 1
// Compile with:
// g++ -Wall -o AS02A-S1 ES4268738-AaRP-AS02A-S1.cpp
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> //fork
#include <unistd.h> //sleep
#include <sys/select.h> //select
#include <string.h>
#include <sys/wait.h> //wait
#include <semaphore.h> //semaphore
#include <fcntl.h> //semaphore
#include "ES4268738-AaRP-AS02.h"

// Global variables
pid_t Pid = getpid();

// Default values
char PidName[] = "Subscriber-1";
char ShortName[20] = "SUB1"; // The size is necessary for the GNU/Linux console codes

// Component specific parameters
int ConditionSub1 = 1;
int loop = 1;

// Signal handler
void SignalHandler(int SignalNumber) {
	printf("[%s][%04d][SIGNAL HANDLER] Process %d received signal: %d\n", ShortName, loop, getpid(), SignalNumber);
	ConditionSub1 = 0;
	// exit(SignalNumber); //~
}

// Main function
int main(int argc, char *argv[])
{
// Register signal handler
signal(SIGUSR1, SignalHandler);

// Semaphores
sem_t *semaphore0 = sem_open("semaphoremediator", O_CREAT|O_EXCL, 0, 1);
sem_unlink("semaphoremediator"); // Prevent a stray semaphore
// sem_t *semaphore1 = sem_open("semaphoreclients", O_CREAT|O_EXCL, 0, 1);
// sem_unlink("semaphoreclients"); // Prevent a stray semaphore

// General parameters
// Default values
int TimePeriod = 3000;
int MessageLength = 1;
int Pipe[4];

// Assign command/call parameters
if (argc > 1) {
	sscanf(argv[1], "%s", &PidName);
	sscanf(argv[2], "%s", &ShortName);
	sscanf(argv[3], "%d", &TimePeriod);
	sscanf(argv[4], "%d", &MessageLength);
	sscanf(argv[5], "%d", &Pipe[0]); // Pipe to mediator
	sscanf(argv[6], "%d", &Pipe[1]); // Pipe to mediator
	sscanf(argv[7], "%d", &Pipe[2]); // Pipe from mediator
	sscanf(argv[8], "%d", &Pipe[3]); // Pipe from mediator
} else {
	printf("[%s][INFO] -- %s: Using default values\n", ShortName, loop, PidName);
}

printf("[%s][%04d] -- %s initialization\n", ShortName, loop, PidName);
printf("[%s][INFO] -- PID: %d\n", ShortName, Pid);
printf("[%s][INFO] -- Time period: %d\n", ShortName, TimePeriod);
printf("[%s][INFO] -- Message length: %d\n", ShortName, MessageLength);
printf("[%s][INFO] -- Pipe %dR: %d\n", ShortName, 1, Pipe[0]);
printf("[%s][INFO] -- Pipe %dW: %d\n", ShortName, 1, Pipe[1]);
printf("[%s][INFO] -- Pipe %dR: %d\n", ShortName, 2, Pipe[2]);
printf("[%s][INFO] -- Pipe %dW: %d\n", ShortName, 2, Pipe[3]);

// Define the message (buffer for writing to pipe)
int bw = 0; // Number of bytes written
int br = 0; // Number of bytes read
char Message[MessageLength];
char RequestMessage[MessageLength] = "REQ1"; // Unique character set
RequestMessage[MessageLength] = '\0';

// Initial delay
usleep(TimePeriod * 1000); // usleep used for emulating msleep
// usleep(TimePeriod * Pid); // usleep used for emulating msleep

// /* Set parameters for select() //~POLLING
// Prepare select parameters
fd_set rfds;
int nActivePipeFD;
int nfds = Pipe[2];
// */
int ActivePipeFD = Pipe[1];
struct timeval tv;

// /* Semaphore implementation //~SEMAPHORE
printf("[%s][INFO] -- Awaiting semaphore release\n", ShortName);
sem_wait(semaphore0); // Waiting for semaphore release from mediator
printf("[%s][INFO] -- Semaphore released\n", ShortName);
// */

// Main loop
do {
// Set loops period in usec
tv.tv_usec = TimePeriod * 1000;

// /* Set parameters for select() //~POLLING
FD_ZERO(&rfds);
// FD_SET(Pipe[0], &rfds);
FD_SET(Pipe[2], &rfds); // Only monitoring this pipe would be necessary
// */

// Write a message request to the mediator via provided pipe FD
printf("[%s][%04d] -- Requesting message...\n", ShortName, loop);
printf("[%s][%04d] -- Writing %d bytes to pipe (FD %d)\n", ShortName, loop, MessageLength, Pipe[1]);
bw = write(Pipe[1], &RequestMessage, sizeof(RequestMessage));
printf("[%s][%04d] -- %d bytes written to pipe (FD %d)\n", ShortName, loop, bw, Pipe[1]);
if (bw == MessageLength) {
// /* Polling active pipes using select //~POLLING
// This is deactivated since the scheme is simple and there is no need for the select() here
// However for completeness sake it remains for possible future use in a different scenario
// Another issue is that the subscriber might get stuck in the reading process preventing it from exiting properly
	usleep(TimePeriod * 100); // Wait a little longer for mediator; 1/10 of TP
	nActivePipeFD = select(nfds + 1, &rfds, NULL, NULL, &tv);
	if (nActivePipeFD < 0) {
		perror("select()");
		printf("[%s][ERROR] PID %d select() error: %d\n", ShortName, Pid, ActivePipeFD); // 
	} else if (nActivePipeFD == 0) printf("[%s][%04d] -- No active pipe actions detected\n", ShortName, loop);
	else for (int i = 2; i < 4; i = i + 2) { // There is only one pipe and FD to monitor in this case
		ActivePipeFD = Pipe[i];
		// printf("[%s][%04d] -- Active pipes (# FD %d)\n", ShortName, loop, nActivePipeFD); //~DEBUG
		// printf("[%s][%04d] -- Active pipes (FD %d|%d)\n", ShortName, loop, Pipe[0], Pipe[2]); //~DEBUG
		printf("[%s][%04d] -- Active pipe (FD %d)\n", ShortName, loop, ActivePipeFD);
		// if (FD_ISSET(Pipe[2], &rfds)) { //~IPOLLING
// */
	// printf("[%s][%04d] -- Active pipe (FD %d)\n", ShortName, loop, ActivePipeFD); //~IPOLLING
			printf("[%s][%04d] -- Awaiting message...\n", ShortName, loop);
			printf("[%s][%04d] -- Reading %d bytes from pipe (FD %d)\n", ShortName, loop, MessageLength, Pipe[2]);
			br = read(Pipe[2], &Message, sizeof(Message));
			if (br == MessageLength) {
				printf("[%s][%04d] -- %d bytes read from pipe (FD %d)\n", ShortName, loop, br, Pipe[2]);
				printf("[%s][%04d] -- Subscribing message: [%s]\n", ShortName, loop, Message);
			}
			// else usleep(TimePeriod * 100); // Wait a little longer
		// } //~IPOLLING
// /* Polling closing braces //~POLLING
	}
}
// */

// Assume zero delay in producing and transmitting data
// TimePeriod pseudo corresponds to the period of the component
// usleep(TimePeriod * 1000); // usleep used for emulating msleep
usleep(tv.tv_usec); // Sleep for remaining time; usleep used for emulating msleep

loop++; // Increase loop counter each cycle
} while (ConditionSub1);

// Exit message after receiving signal from signal handler
printf("[%s][%04d] PID %d: Exiting %s\n", ShortName, loop, Pid, PidName);

// Based on the last reading cycle determine the exit value that will be passed to the initializer
if (br == MessageLength) return 0; // For complete message transfer
else return 1; // For incomplete message transfer
// exit(0); //~
}
