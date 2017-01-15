/*
Assignment 02 - Advanced and Robot Programming
Publisher, subscriber and mediator scheme in POSIX environment
Ernest Skrzypczyk -- 4268738 -- EMARO+ // ES4268738-AaRP-AS02
*/
// Publisher 1
// Compile with:
// g++ -Wall -o AS02A-P1 ES4268738-AaRP-AS02A-P1.cpp
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> //fork
#include <unistd.h> //sleep
#include <string.h>
#include <sys/wait.h> //wait
#include <semaphore.h> //semaphore
#include <fcntl.h> //semaphore
#include "ES4268738-AaRP-AS02.h"

// Global variables
pid_t Pid = getpid();

// Default values
char PidName[] = "Publisher-1";
char ShortName[20] = "PUB1"; // The size is necessary for the GNU/Linux console codes

// Component specific parameters
int ConditionPub1 = 1;
int loop = 1;

// Signal handler
void SignalHandler(int SignalNumber) {
	printf("[%s][%04d][SIGNAL HANDLER] Process %d received signal: %d\n", ShortName, loop, getpid(), SignalNumber);
	ConditionPub1 = 0;
	// exit(SignalNumber); //~
}

// Random character generation from given charset
void GenerateRandomCharacter(char *charset, int s, int l, char *message) {
	int t; char c[l];
	// int t; char c[l + 1]; //~ Not necessary
	// printf("[%s][%04d] -- Array size: %zu; Message length: %d; Message size: %d\n", ShortName, loop, sizeof(charset), l, sizeof(c));

	for (int i = 0; i < l; ++i) {
		t = (rand() * Pid) % s;
		if (t < 0) t *= -1; // Make sure the pseudo random number is positive
		// printf("[%s][%04d] -- Generated pseudo random number: %d; \n", ShortName, loop, t);
		c[i] = charset[t];
		// printf("[%s][%04d] -- Generated random character: [%c]\n", ShortName, loop, c[i]);
	}
	// c[l + 1] = '\0'; // Set last character to null //~ Automatically appended
	// printf("[%s][%04d] -- Generated message: [%s]\n", ShortName, loop, c);
	for (int i = 0; i < l; ++i) message[i] = c[i]; // Assign generated values one by one
	// printf("[%s][%04d] -- Generated message: [%s]\n", ShortName, loop, message);
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
int TimePeriod = 1000;
int MessageLength = 1;
int Pipe[2];

// Assign command/call parameters
if (argc > 1) {
	sscanf(argv[1], "%s", &PidName);
	sscanf(argv[2], "%s", &ShortName);
	// char tmp[32] = "";
	// sscanf(argv[2], "%s", &tmp);
	// if (strcmp(tmp, "PUB1") == 0)
		// sprintf(ShortName, "%s%s%s", CP1, &tmp, CR);
	// else
		// sprintf(ShortName, "%s%s%s", CP2, &tmp, CR);

	sscanf(argv[3], "%d", &TimePeriod);
	sscanf(argv[4], "%d", &MessageLength);

	sscanf(argv[5], "%d", &Pipe[1]); // Pipe to mediator - unidirectional
} else {
	printf("[%s][INFO] -- %s: Using default values\n", ShortName, loop, PidName);
}

printf("[%s][%04d] -- %s initialization\n", ShortName, loop, PidName);
printf("[%s][INFO] -- PID: %d\n", ShortName, Pid);
printf("[%s][INFO] -- Time period: %d\n", ShortName, TimePeriod);
printf("[%s][INFO] -- Message length: %d\n", ShortName, MessageLength);
printf("[%s][INFO] -- Pipe %dW: %d\n", ShortName, 1, Pipe[1]);

// Alphabet definition - Upper characters // String literal, could be also char *AlphabetUpper = "..."
char AlphabetUpper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Alphabet definition - Lower characters
char AlphabetLower[] = "abcdefghijklmnopqrstuvwxyz";

// Digits definition
char Digits[] = "0123456789";

// Define the message (buffer for writing to pipe)
int bw = MessageLength; // Number of written bytes
char Message[MessageLength];

// Initial delay
usleep(TimePeriod * 1000); // usleep used for emulating msleep
// usleep(TimePeriod * Pid); // usleep used for emulating msleep

// /* Semaphore implementation //~SEMAPHORE
printf("[%s][INFO] -- Awaiting semaphore release\n", ShortName);
sem_wait(semaphore0); // Waiting for semaphore release from mediator
printf("[%s][INFO] -- Semaphore released\n", ShortName);
// */

// Main loop
do {
// Depending on the name of the passed process name, generate a random character from on of the sets
if (strcmp(PidName, "Publisher-1") == 0) GenerateRandomCharacter(AlphabetLower, (int) (sizeof(AlphabetLower) - 1), MessageLength, Message);
else if (strcmp(PidName, "Publisher-2") == 0) GenerateRandomCharacter(AlphabetUpper, (int) (sizeof(AlphabetUpper) - 1), MessageLength, Message);
else GenerateRandomCharacter(Digits, (int) (sizeof(Digits) - 1), MessageLength, Message);

// Write the generated message to the provided pipe FD
// printf("[%s%s%s][%04d] -- Publishing message: [%s]\n", CP, ShortName, CR, loop, Message);
printf("[%s][%04d] -- Publishing message: [%s]\n", ShortName, loop, Message);
// printf("[%s][%04d] -- Writing %d bytes to pipe (FD %d)\n", ShortName, loop, sizeof(Message), Pipe[1]);
printf("[%s][%04d] -- Writing %d bytes to pipe (FD %d)\n", ShortName, loop, MessageLength, Pipe[1]);
// Message[MessageLength + 1] = '\n';
bw = write(Pipe[1], &Message, sizeof(Message));
printf("[%s][%04d] -- %d bytes written to pipe (FD %d)\n", ShortName, loop, bw, Pipe[1]);

// Assume zero delay in producing and transmitting data
// TimePeriod pseudo corresponds to the period of the component
usleep(TimePeriod * 1000); // usleep used for emulating msleep

loop++; // Increase loop counter each cycle
} while (ConditionPub1);

// Exit message after receiving signal from signal handler
printf("[%s][%04d] PID %d: Exiting %s\n", ShortName, loop, Pid, PidName);

// Based on the last writing cycle determine the exit value that will be passed to the initializer
if (bw == MessageLength) return 0; // For complete message transfer
else return 1; // For incomplete message transfer
// exit(0); //~
}
