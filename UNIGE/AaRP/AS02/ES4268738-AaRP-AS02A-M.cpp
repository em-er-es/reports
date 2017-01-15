/*
Assignment 02 - Advanced and Robot Programming
Publisher, subscriber and mediator scheme in POSIX environment
Ernest Skrzypczyk -- 4268738 -- EMARO+ // ES4268738-AaRP-AS02
*/
// Mediator
// Compile with:
//	g++ -Wall -o AS02A-M ES4268738-AaRP-AS02A-M.cpp
#include <cmath> //ceil
#include "ES4268738-AaRP-AS02.h"
#include <fcntl.h> //semaphore
#include <iostream>
// #include <limits.h> //UCHAR_MAX
#include <pthread.h> //fork
#include <semaphore.h> //semaphore
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h> //select
#include <sys/time.h> //time
#include <sys/types.h>
#include <sys/wait.h> //wait
#include <unistd.h> //sleep

// Global variables
pid_t Pid = getpid();

// Default values
char PidName[] = "Mediator";
// char ShortName[20] = "MEDI"; //~COLOR
char ShortName[20] = CM"MEDI"CR; // Manual color version //~COLOR

// Component specific parameters
int ConditionMedi = 1;
int loop = 1;

// Signal handler
void SignalHandler(int SignalNumber) {
	printf("[%s][%04d][SIGNAL HANDLER] Process %d received signal: %d\n", ShortName, loop, getpid(), SignalNumber);
	ConditionMedi = 0;
}

// Function declarations
int FDSets(bool fdsinit, fd_set &rfds, fd_set &wfds, int Pipes[][2], int npipes);

void ListPFD(fd_set &rfds, fd_set &wfds, int Pipes[][2], int &npipes);

// Main function
int main(int argc, char *argv[])
{
// Register signal handler
signal(SIGUSR1, SignalHandler);

// Semaphores -- Used only for initial synchronization
sem_t *semaphore0 = sem_open("semaphoremediator", O_CREAT|O_EXCL, 0, 1);
sem_unlink("semaphoremediator"); // Prevent a stray semaphore

// General parameters
// Default values
int TimePeriod = 100;
int DecayTime = 7000;
int MessageCounter = 0;
int RelativeMessageCounter = 0;
int MessageLength = 1;
int MessageBuffer = MessageLength * 8;
int alignment = 2;
int nPublisher = 2;
int nSubscriber = 3;
int npipes = nPublisher + nSubscriber * 2; // Subscribers take twice as many pipes in this case
int Request = 0;
int RequestCounter = 0;
int Pipes[npipes][2];

// Information
printf("[%s][INFO] -- %s initialization\n", ShortName, PidName);

// Assign command/call parameters
if (argc > 1) {
	sscanf(argv[1], "%s", &PidName);
	sscanf(argv[2], "%s", &ShortName);
	sscanf(argv[3], "%d", &TimePeriod);
	sscanf(argv[4], "%d", &MessageLength);
	sscanf(argv[5], "%d", &MessageBuffer);

	// Pipes to mediator
	sscanf(argv[6], "%d", &npipes);
	///TODO - Pipes should be recalculated based on pipe FDs; Either pass on the argument additionaly or calculate/detect directly based on FDs
	// nPublisher = 0; nSubscriber = 0;
	for (int i = 0; i < npipes; i++) {
		sscanf(argv[i + 7], "%d", &Pipes[i][0]);
		// nPublisher++;
	}
	for (int i = 0; i < npipes; i++) {
		sscanf(argv[i + npipes + 7], "%d", &Pipes[i][1]);
		// nSubscriber++;
	}

	// Decay time for the circular buffer
	sscanf(argv[23], "%i", &DecayTime);
} else {
printf("[%s][INFO] -- %s: Using default values\n", ShortName, PidName);
}

// Time periods and ratio
int TimePeriods[5] = {PUB1_PERIOD, PUB2_PERIOD, SUB1_PERIOD, SUB2_PERIOD, SUB3_PERIOD};
float mintp = TimePeriods[0]; float maxts = TimePeriods[2];
// for (int i = 0; i < 5; i++) printf("TP %d\n", TimePeriods[i]); //~DEBUG
for (int i = 1; i < 5; i++) {
	if (TimePeriods[i] < mintp) mintp = TimePeriods[i];
	if (TimePeriods[i] > maxts) maxts = TimePeriods[i];
}
float tpratio = maxts / mintp;
// Setting the circular buffer length according to time periods
// ratio of maximum subscriber period to minimum publisher period
// if (tpratio > 0) MessageBuffer = (int) (MessageLength * ceil(tpratio / 8) * 8) + ceil((float) nPublisher / 8) * 8 + ceil((float) nSubscriber / 8) * 8; //~VERBOSE
// if (tpratio > 0) MessageBuffer = (MessageLength * ceil(tpratio / 8) * 8) + ceil(nPublisher / 8) * 8 + ceil(nSubscriber / 8) * 8; // Used for the more efficient storage approach
// if (tpratio > 0) MessageBuffer = (MessageLength + nPublisher + nSubscriber) * ceil(tpratio / 8) * 8;
int MessageFrame = MessageLength + nPublisher + nSubscriber + alignment; // Taking into account the necessary offset for char arrays, otherwise risk of data inconsistency
if (tpratio > 0) MessageBuffer = MessageFrame * ceil(tpratio / 8) * 8;
int MessageSlots = MessageBuffer / MessageFrame;

// Information display
printf("[%s][INFO] -- Pid: %d\n", ShortName, Pid);
printf("[%s][INFO] -- Time period: %d\n", ShortName, TimePeriod);
printf("[%s][INFO] -- Time ratio (S_max = %g / P_min = %g): %g\n", ShortName, maxts, mintp, ceil(tpratio));
printf("[%s][INFO] -- Message length: %d\n", ShortName, MessageLength);
printf("[%s][INFO] -- Message buffer: %d\n", ShortName, MessageBuffer);
printf("[%s][INFO] -- Message slots: %d\n", ShortName, MessageSlots);
printf("[%s][INFO] -- Message decay time: %d\n", ShortName, DecayTime);
// printf("[%s][INFO] -- Message buffer: %d \t# (MessageLength * ceil(Time Period Ratio) %d * %f)\n", ShortName, MessageBuffer, MessageLength, ceil(tpratio / 8) * 8); //~VERBOSE

// /* List all pipes passed to execl //~VERBOSE
for (int i = 0; i < npipes; i++) {
	printf("[%s][INFO] -- Pipe %dR: %d\n", ShortName, i + 1, Pipes[i][0]);
	printf("[%s][INFO] -- Pipe %dW: %d\n", ShortName, i + 1, Pipes[i][1]);
}
// */

// Message
char Message[MessageLength];

// Circular buffer
char CircularBuffer[MessageBuffer];

// char MetaData[npipes * MessageSlots + 1]; // Another buffer to store metadata as an alternative
// int MetaData[npipes * MessageSlots]; // Another buffer of another type to store metadata as an alternative
int mb; // MediatorBuffer() return value

// The main buffer operations function MediatorBuffer() declaration, depending on situation passed variables used differently
int MediatorBuffer(char *Message, int MessageLength, char *CircularBuffer, int MessageBuffer, int &RelativeMessageCounter, int nPublisher, int nSubscriber, int Pipes[][2], int PipeFD, int Mode);
mb = MediatorBuffer(0, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, 0, 0, 5); // MediatorBuffer() return value

// Number of bytes read and written
int br = 0;
int bw = 0;

// Prepare select parameters
fd_set rfds, wfds;
struct timeval tv;
int ActivePipeFD;
int nActivePipeFD;
int nfds;

/* Used to list all available pipe FDs in both sets after interpreting passed arguments //~VERBOSE
int nfds = FDSets(1, rfds, wfds, Pipes, npipes);
ListPFD(rfds, wfds, Pipes, npipes);
// */

// Initial delay
// Time variables
struct timeval itv;
struct timezone itz;
struct timeval ctv;
struct timezone ctz;

// Semaphores used for initial synchronasation
printf("[%s][INFO] -- Preparing semaphore release\n", ShortName); //~SEMAPHORE
// usleep(TimePeriod * 10 * 1000); // usleep used for emulating msleep
usleep(TimePeriod * 15 * 1000); // usleep used for emulating msleep
// usleep(TimePeriod * 20 * 1000); // usleep used for emulating msleep
// usleep(TimePeriod * 30 * 1000); // usleep used for emulating msleep

printf("[%s][INFO] -- Releasing semaphore\n", ShortName); //~SEMAPHORE
// sem_post(semaphore1); // Releasing semaphore for clients //~SEMAPHORE
sem_post(semaphore0); // Releasing semaphore for clients
// sem_post(semaphore0); // Releasing semaphore for clients

// /* Get initialization time //~TIME
gettimeofday(&itv, &itz);
printf("[%s][INFO] -- Initialization time (usec): %d\n", ShortName, itv.tv_usec); //~DEBUG
// */

// Main loop
do {
// Set loops period in usec
tv.tv_usec = TimePeriod * 1000;
// tv.tv_usec = mintp * 1000;

// Circular buffer
// Initialize file descriptor sets
nfds = FDSets(1, rfds, wfds, Pipes, npipes);
// Polling active pipes using select
nActivePipeFD = select(nfds + 1, &rfds, NULL, NULL, &tv); // Initially check only pipe FDs from the reading set
// List active pipes
ListPFD(rfds, wfds, Pipes, npipes); //~DEBUG

if (nActivePipeFD < 0) { // Error checking routine
	// perror("select()"); //~DEBUG // select() must be last call
	printf("[%s][ERROR] PID %d select() error: %d\n", ShortName, Pid, ActivePipeFD); // Using printf instead of perror, since other calls located in between
	exit(1); // Error value exit call
} else if (nActivePipeFD == 0) printf("[%s][%04d] -- No active pipe actions detected\n", ShortName, loop);
else for (int j = 0; j < 2; j++) {
	for (int i = 0; i < npipes; i++) {
		// printf("%d, %d\n", i, j); //~DEBUG
		ActivePipeFD = Pipes[i][j];
		// printf("[%s][%04d] -- Checking pipe (FD %d)\n", ShortName, loop, ActivePipeFD); //~VERBOSE
		if (FD_ISSET(ActivePipeFD, &rfds)) {
			// Read
			printf("[%s][%04d] -- Receiving message from pipe (FD %d)...\n", ShortName, loop, ActivePipeFD);
			br = read(ActivePipeFD, &Message, sizeof(Message));
			// if (br == MessageLength) MessageCounter++; else return -1; // Error thrown for inproper message transmission
			if (br == MessageLength) MessageCounter++; else {
				printf("[%s][%04d] -- %d bytes read from pipe (FD %d) where %d bytes expected; Skipping\n", ShortName, loop, br, ActivePipeFD, MessageLength); // Error message for inproper message transmission
			}
			printf("[%s][%04d] -- %d bytes read from pipe (FD %d)\n", ShortName, loop, br, ActivePipeFD);
			printf("[%s][%04d] -- Received message #%d: [%s]\n", ShortName, loop, MessageCounter, Message);

			// Check if message is a request from subscriber
			if (!strcmp(Message, "REQ1")) { //Unique string for message request, not possible from withing provided publisher character sets
				int sid = 0;
				// for (int i = 0; i < nSubscriber * 2; i++) printf("Pipes[np + %d * 2] == %d (APFD == %d)\n", i, *Pipes[nPublisher + i * 2], ActivePipeFD); //~DEBUG
				for (int i = 0; i < nSubscriber * 2; i++) if (*Pipes[nPublisher + i * 2] == ActivePipeFD) sid = i; // Simply scan all pipe FDs, even though only every i * 2 could be scanned, since they are ordered for read and write operations
				printf("[%s][%04d] -- Request received [%s] from subscriber %d at pipe (FD %d)\n", ShortName, loop, Message, sid + 1, ActivePipeFD);
				Request = ActivePipeFD;
				RequestCounter++;
				// continue; // Continue with next loop; Could be difficult if more than 1 requests are given at the same time
				break; // Break operations, serve subscriber first
			}

			// Put message into circular buffer with appropriate metadata
			printf("[%s][%04d] -- Sending message [%s] to circular buffer...\n", ShortName, loop, Message);
			mb = MediatorBuffer(Message, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, Pipes, ActivePipeFD, 1);
			if (! mb) printf("[%s][%04d] -- Message #%d [%s] saved successfully in circular buffer...\n", ShortName, loop, MessageCounter, Message); // Message counter increased within function call
		}

		// Handle subscriber requests
		if (Request) {
				
		ActivePipeFD = Request + 3;
		// Check if there is a message for ActivePipeFD subscriber
		// printf("[%s][%04d] -- Processing message request from pipe (FD %d)\n", ShortName, loop, ActivePipeFD - 3);
		printf("[%s][%04d] -- Processing message request from pipe (FD %d)\n", ShortName, loop, Request);
		// printf("[%s][%04d] -- Rel Msg Counter %d)\n", ShortName, loop, RelativeMessageCounter);//~DEBUG
		mb = MediatorBuffer(Message, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, Pipes, ActivePipeFD, 3);
		ActivePipeFD = mb;
		// printf("[%s][%04d] -- Return value: %d\n", ShortName, loop, mb);//~DEBUG
		// printf("[%s][%04d] -- Message: [%s]\n", ShortName, loop, Message);//~DEBUG
		// MediatorBuffer(Message, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, Pipes, RelativeMessageCounter, 4); // Print message in buffer
		MediatorBuffer(Message, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, Pipes, -1, 4); // Print messages in buffer
		printf("[%s][%04d] -- Processing message %d towards pipe (FD %d)\n", ShortName, loop, RelativeMessageCounter, ActivePipeFD);
		if (ActivePipeFD) {
			// Write
			printf("[%s][%04d] -- Sending message [%s] to pipe (FD %d)...\n", ShortName, loop, Message, ActivePipeFD);
			bw = write(ActivePipeFD, &Message, sizeof(Message));
			// bw = write(ActivePipeFD + 1, &Message, sizeof(Message)); //~2D
			if (bw == MessageLength) {
				printf("[%s][%04d] -- %d bytes written to pipe (FD %d)\n", ShortName, loop, bw, ActivePipeFD);
				RequestCounter--; // Decrease request counter
				printf("[%s][%04d] -- Requests remaining %d\n", ShortName, loop, RequestCounter);
				// Update metadata
				mb = MediatorBuffer(Message, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, Pipes, ActivePipeFD, 9);
				MediatorBuffer(Message, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, Pipes, ActivePipeFD, 4); // Print message in buffer after updating
			} else {
				printf("[%s][%04d] -- %d bytes read from pipe (FD %d) where %d bytes expected; Skipping\n", ShortName, loop, br, ActivePipeFD, MessageLength); // Error message for inproper message transmission
			}
			// printf("[%s][%04d] -- %d bytes written to pipe %d\n", ShortName, loop, bw, ActivePipeFD);
			// printf("[%s][%04d] -- Transmitted message: %s\n", ShortName, loop, Message);

			Request = 0; // Reset request flag
			printf("[%s][%04d] -- Message [%s] processed\n", ShortName, loop, Message);
			// Print buffer after done
			MediatorBuffer(Message, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, Pipes, -1, 4);
			}
		}
	}
}

/* Test deleting messages from circular buffer //~DEBUG //~TEST
if (loop == 15) {
	printf("[%s][%04d] -- Message deletion test in process...\n", ShortName, loop);
	MediatorBuffer(0, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, 0, -1, 4);
	MediatorBuffer(0, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, 0, 4, 2);
	MediatorBuffer(0, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, 0, -2, 4);
	MediatorBuffer(0, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, 0, 2, 2);
	MediatorBuffer(0, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, 0, -2, 4);
	MediatorBuffer(0, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, 0, 1, 2);
	MediatorBuffer(0, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, 0, -1, 4);
	printf("[%s][%04d] -- Message deletion test finished...\n", ShortName, loop);
}
// */

// List buffer content all 10 loops
if (!(loop % 10))
	mb = MediatorBuffer(Message, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, Pipes, -1, 4);

// Search for served messages and delete them; Handled within MediatorBuffer()
mb = MediatorBuffer(Message, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, Pipes, ActivePipeFD, 3);

// /* Get current time //~TIME
gettimeofday(&ctv, &ctz);
printf("[%s][INFO] -- Current time (usec): %d\n", ShortName, ctv.tv_usec); //~DEBUG
printf("[%s][INFO] -- Time difference: %d\n", ShortName, abs(itv.tv_usec - ctv.tv_usec));
if (abs(itv.tv_usec - ctv.tv_usec) > DecayTime * 1000) {
	printf("[%s][INFO] -- Time decay for messages surpassed: %d > %d\n", ShortName, DecayTime, abs(itv.tv_usec - ctv.tv_usec));
	mb = MediatorBuffer(Message, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, Pipes, ActivePipeFD, 5);
	gettimeofday(&itv, &itz);
	printf("[%s][INFO] -- Initialization time (usec): %d\n", ShortName, itv.tv_usec); //~DEBUG
}
// */

/* Not necessary, since select() allows polling
// Assume zero delay in producing and transmitting data
// TimePeriod pseudo corresponds to the period of the component
// usleep(TimePeriod * 1000); // usleep used for emulating msleep
// usleep(tv.tv_usec * 1000); // usleep used for emulating msleep
*/
usleep(tv.tv_usec); // Sleep for remaining period time, usleep used for emulating msleep

// ConditionMedi = 0;//~DEBUG // One time loop
loop++; // Increase loop counter each cycle
} while (ConditionMedi);

sem_post(semaphore0); //~SEMAPHORE // Releasing semaphore for stuck zombie clients
printf("[%s][%04d] PID %d: Exiting %s\n", ShortName, loop, Pid, PidName);
// return 2; // Exit calls return N; or exit(N);
// exit(0); // Default exit call
exit(2); // Show error handling is correct in INIT
}

// Function definitions
// File descriptor sets operations
int FDSets(bool fdsinit, fd_set &rfds, fd_set &wfds, int Pipes[][2], int npipes) {
	int nfds = 0; // Maximum number of file descriptors initialization
	if (fdsinit) {
	// printf("[%s][%04d] -- Initializing FD sets\n", ShortName, loop); //~DEBUG

	// Clear file descriptor sets for read and write
	FD_ZERO(&rfds); // Read file descriptors set
	FD_ZERO(&wfds); // Write file descriptors set

		// Define both FD sets
		for (int i = 0; i < 2; i++) { // Add only publisher reading pipe FDs
			FD_SET(Pipes[i][0], &rfds);
		// printf("[%s][%04d] -- Adding FD %d to reading set\n", ShortName, loop, Pipes[i][0]); //~DEBUG
		}
		for (int i = 0; i < 3; i++) { // Add only subscriber reading pipe FDs
			FD_SET(Pipes[2 + i * 2][0], &rfds);
		// printf("[%s][%04d] -- Adding FD %d to reading set\n", ShortName, loop, Pipes[i][0]); //~DEBUG
		}
		for (int i = 0; i < npipes; i++) { // Add all writing pipe FDs
			FD_SET(Pipes[i][1], &wfds);
		// printf("[%s][%04d] -- Adding FD %d to writing set\n", ShortName, loop, Pipes[i][1]); //~DEBUG
		}

// /* Remove the closed file descriptor from the set
	FD_CLR(Pipes[3][0], &rfds); // Pipe 4: remove the reading end from set
	FD_CLR(Pipes[5][0], &rfds); // Pipe 6: remove the reading end from set
	FD_CLR(Pipes[7][0], &rfds); // Pipe 8: remove the reading end from set
	FD_CLR(Pipes[0][1], &wfds); // Pipe 1: remove the writing end from set
	FD_CLR(Pipes[1][1], &wfds); // Pipe 2: remove the writing end from set
	FD_CLR(Pipes[2][1], &wfds); // Pipe 3: remove the writing end from set
	FD_CLR(Pipes[4][1], &wfds); // Pipe 5: remove the writing end from set
	FD_CLR(Pipes[6][1], &wfds); // Pipe 7: remove the writing end from set
// */
	}
// Initialization end

	// Calculate current maximum file descriptor
	for (int i = 0; i < npipes; i++) {
		if ((Pipes[i][0] > nfds) && (FD_ISSET(Pipes[i][0], &rfds))) nfds = Pipes[i][0];
		if ((Pipes[i][1] > nfds) && (FD_ISSET(Pipes[i][1], &wfds))) nfds = Pipes[i][1];
	}

	return nfds; // Return calculate nfds
}

// List all file descriptors in sets provided
void ListPFD(fd_set &rfds, fd_set &wfds, int Pipes[][2], int &npipes) {
	// List FD in sets
	char M[2] = {'R', 'W'};
	for (int j = 0; j < 2; j++) {
		printf("[%s][%04d] -- Active pipes[*][%d] %c: FD ", ShortName, loop, j, M[j]); //~DEBUG
		for (int i = 0; i < npipes; i++) {
			if ((j == 0) && (FD_ISSET(Pipes[i][j], &rfds))) printf("%d ", Pipes[i][j]); //~DEBUG
			if ((j == 1) && (FD_ISSET(Pipes[i][j], &wfds))) printf("%d ", Pipes[i][j]); //~DEBUG
		}
		printf("\n"); //~DEBUG
	}
}

// Mediator circular buffer implementation
/// TODO - There are still some ideas worth implementing; Futher optimization from within function and main() calls possible
int MediatorBuffer(char *Message, int MessageLength, char *CircularBuffer, int MessageBuffer, int &RelativeMessageCounter, int nPublisher, int nSubscriber, int Pipes[][2], int PipeFD, int Mode) {
	// Local variables
	char msg[MessageLength]; // Local copy of the message
	int npipes = nPublisher + nSubscriber; // Only metadata relevant pipes
	// char mp[nPublisher + 1]; // Size depends on number of publishers directly, one byte per publisher
	// char ms[nSubscriber + 1]; // This initialization because of later alignment not necessary
	char mp[nPublisher]; // Size depends on number of publishers directly, one byte per publisher
	char ms[nSubscriber];
	char md[nPublisher + nSubscriber + 1]; // Total metadata
	int offset = 0; // Additional offsets for futher modification
	int oalignment = 2; // Total alignment offset
	int alignment = oalignment - 1; // Message -- Metadata offset
	int lRelativeMessageCounter = RelativeMessageCounter;
	int MessageSlots = MessageBuffer / (MessageLength + npipes + oalignment);
	int ec, mb, lc;
	int SourcePipe[nPublisher], TargetPipe[nSubscriber];
	int MessageFrame = MessageLength + nPublisher + nSubscriber + oalignment;
	int DeleteMessage = -1;
	int mf = 0; int me = MessageSlots;
	// int moffset = MessageLength / 8 + 1; // For the more memory efficient approach
	int moffset = 0; // Offset for messages
	// int RelativeMessageCounter = MessageCounter % MessageSlots;
	/* Additional information on the circular buffer //~DEBUG //~VERBOSE
	printf("[%s][%04d] -- Circular buffer variables: Mode %d\n", ShortName, loop, Mode);
	printf("[%s][%04d] -- Circular buffer variables: Message: [%s], %d bytes long\n", ShortName, loop, Message, MessageLength);
	printf("[%s][%04d] -- Circular buffer variables: Circular buffer: &[%d], %d bytes long, message slots: %d\n", ShortName, loop, &CircularBuffer, MessageBuffer, MessageSlots);
	printf("[%s][%04d] -- Circular buffer variables: Message counter: %d, publishers : %d, subscribers : %d\n", ShortName, loop, RelativeMessageCounter, nPublisher, nSubscriber);
	printf("[%s][%04d] -- Circular buffer variables: Number of pipes: %d, pipe FD given: %d\n", ShortName, loop, npipes, PipeFD);
	// */

	// Operation modes
	switch (Mode) {
		case 0:
		///FIX - Blocking
		// Read message from the circular buffer; Mostly implemented in other cases
			lRelativeMessageCounter = RelativeMessageCounter;
			// printf("[%s][%04d] -- Circular buffer: Mode %d\n", ShortName, loop, Mode); //~VERBOSE
			printf("[%s][%04d] -- Circular buffer: Reading message slot #%d from circular buffer\n", ShortName, loop, lRelativeMessageCounter); //~DEBUG

				// Reading message and metadata
				for (int i = 0; i < MessageSlots; i++) {
					// for (int j = 0; j < sizeof(msg) + sizeof(mp) + sizeof(ms) + oalignment; j++) {
					for (int j = 0; j < sizeof(msg) + oalignment; j++) {
						if (i == lRelativeMessageCounter) {
							if (j < MessageLength) msg[j] = *(CircularBuffer + i * (MessageLength + npipes + oalignment) + j); // Local copy
							if (j < MessageLength) Message[j] = *(CircularBuffer + i * (MessageLength + npipes + oalignment) + j); // Outside array
						}
						// if ((j == MessageLength) || (j == MessageLength + alignment)) continue;
						if (j == MessageLength) break;
					}
				}

			printf("[%s][%04d] -- Circular buffer: Message [%s] from pipe (FD %d) read in %d bytes from circular buffer\n", ShortName, loop, msg, PipeFD, MessageLength);
			break;
		case 1:
		// Write message to the circular buffer
			// printf("[%s][%04d] -- Circular buffer: Mode %d\n", ShortName, loop, Mode); //~VERBOSE
			// Check if circular buffer is full based on the message counter 
			// if ((MessageCounter % MessageSlots == MessageSlots) { // Decay message
			offset = RelativeMessageCounter * (MessageLength + npipes + oalignment);
			if (offset > (MessageSlots - 1) * (MessageLength + npipes + oalignment)) {
				offset = 0;
				printf("[%s][%04d] -- Circular buffer variables: Message offset reset\n", ShortName, loop); //~DEBUG
			}
			if (RelativeMessageCounter + 1 > MessageSlots) { // Decay message
				printf("[%s][%04d] -- Circular buffer variables: Message #%d offset: %d\n", ShortName, loop, RelativeMessageCounter, offset); //~DEBUG
				printf("[%s][%04d] -- Circular buffer is full (Message counter %d > Message slots %d). Start clearing circular buffer...\n", ShortName, loop, RelativeMessageCounter + 1, MessageSlots);
				MediatorBuffer(0, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, 0, 1, 5);
				RelativeMessageCounter = 0;
			} else printf("[%s][%04d] -- Circular buffer variables: Message #%d offset: %d\n", ShortName, loop, RelativeMessageCounter + 1, offset); //~DEBUG
			// Reconstruct the message
			for (int i = 0; i < MessageLength; i++) msg[i] = *(Message + i);
			msg[MessageLength] = '\0'; // "Zeroing" last character

		///FIX
		/* Store metadata in char in an efficient manner, so each bytes serves as storage for up to 8 information
			// Determening metadata based on pipe FD
			// Use unsigned char
			unsigned char mp[nPublisher / 8]; // Size depends on number of publishers, one byte serves up to 8 publisher metadata
			for (int i = 0; i < nPublisher * 2; i++) {
				// printf("[%s][%04d] -- Circular buffer variables: Pipe FD given: %d, pipe FD checked: %d, iterations: %d\n", ShortName, loop, PipeFD, **(Pipes + i), i); //~DEBUG
				if (PipeFD == **(Pipes + i)) {
					if (PipeFD % 2 == 0) return -1; // This should never occur, since writting operations are comming only from reading end of pipe, uneven numbers
					// itoa((int)pow(2, i), mp[i / 8], 10); // Non standard alternative
					mp[i / 8] = (int) pow(2, i);
					break; // Break out of the loop, only one publisher per message
				}
			}
			unsigned char ms[int (ceil(nSubscriber / 8) * 8)];
			for (int i = 0; i < nSubscriber; i++) ms[i / 8] += (int) pow(2, i);
		// */

		// /* Store metadata in char in an inefficient manner, so each bytes serves as storage for only 1 information
		// Determening metadata based on pipe FD
			for (int i = 0; i < nPublisher * 2; i++) {
				// printf("[%s][%04d] -- Circular buffer variables: Pipe FD given: %d, pipe FD checked: %d, iterations: %d\n", ShortName, loop, PipeFD, **(Pipes + i), i); //~DEBUG
				if (PipeFD == **(Pipes + i)) {
					if (PipeFD % 2 == 0) return -1; // This should never occur, since writting operations are comming only from reading end of pipe, uneven numbers
					// mp[i] = CHAR_MAX;
					mp[i] = 'P';
					// printf("[%s][%04d] -- Circular buffer variables: Pipe metadata generated: %s, content should be: %d, content is: %d\n", ShortName, loop, mp, int(pow(2, i)), *(int*)mp); //~DEBUG
					// break; // Break out of the loop, only one publisher per message
					// Do not go outside the loop, all subscribers need to be checked
				// } else mp[i] = CHAR_MIN;
				} else mp[i] = 'X';
				mp[nPublisher] = '\0';
			}
			// for (int i = 0; i < nSubscriber; i++) ms[i] = CHAR_MAX;
			for (int i = 0; i < nSubscriber; i++) ms[i] = 'S';
				ms[nSubscriber] = '\0';
		// */

			printf("[%s][%04d] -- Circular buffer: Pipe FD received: %d\n", ShortName, loop, PipeFD);
			// printf("[%s][%04d] -- Circular buffer: Writing message #%d [%s] to circular buffer\n", ShortName, loop, RelativeMessageCounter + 1, msg); //~DEBUG
			/*
			lRelativeMessageCounter = RelativeMessageCounter;
			while (*(CircularBuffer + lRelativeMessageCounter * (MessageLength + npipes + oalignment)) != '\0') {
				lRelativeMessageCounter++;
				if (lRelativeMessageCounter > MessageSlots) { if (ec > 0) return -1; lRelativeMessageCounter = 0; ec = 1; }
			}
			// */
			mb = MediatorBuffer(Message, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, 0, 0, 8);
			if (mb < 0) return -1; else lRelativeMessageCounter = mb;

			printf("[%s][%04d] -- Circular buffer: Writing message [%s] to slot #%d to circular buffer\n", ShortName, loop, msg, lRelativeMessageCounter + 1); //~DEBUG
			for (int i = 0; i < MessageLength; i++) { *(CircularBuffer + i + offset) = *(Message + i);} // Actual writing operation
			for (int i = 0; i < (sizeof(mp)); i++) { *(CircularBuffer + i + offset + sizeof(msg) + oalignment) = mp[i];}
			for (int i = 0; i < (sizeof(ms)); i++) { *(CircularBuffer + i + offset + sizeof(msg) + sizeof(mp) + oalignment) = ms[i];}
			RelativeMessageCounter++;

			printf("[%s][%04d] -- Circular buffer: Message #%d [%s] from pipe (FD %d) written in %d bytes to circular buffer\n", ShortName, loop, lRelativeMessageCounter + 1, msg, PipeFD, MessageLength);
			MediatorBuffer(0, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, 0, lRelativeMessageCounter, 4);
			// MediatorBuffer(0, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, 0, lRelativeMessageCounter, 0);
			printf("[%s][%04d] -- Circular buffer: %d messages in the circular buffer\n", ShortName, loop, RelativeMessageCounter);
			break;
		case 2:
		// Delete message from the circular buffer
			// printf("[%s][%04d] -- Circular buffer: Mode %d\n", ShortName, loop, Mode); //~VERBOSE
			printf("[%s][%04d] -- Circular buffer: Deleting message #%d from the circular buffer\n", ShortName, loop, PipeFD + 1);
			if (*(CircularBuffer + PipeFD * (MessageLength + npipes + oalignment)) != '\0') {
				memset(CircularBuffer + PipeFD * (MessageLength + npipes + oalignment), 0, (MessageLength + npipes + alignment));
				RelativeMessageCounter--;
			} else printf("[%s][%04d] -- Circular buffer: Message #%d in the circular buffer already empty\n", ShortName, loop, PipeFD + 1);
			printf("[%s][%04d] -- Circular buffer: %d messages left in the circular buffer\n", ShortName, loop, RelativeMessageCounter);
			break;
		case 3:
		// Determine next ready message and metadata for message from the circular buffer
			// printf("[%s][%04d] -- Circular buffer: Mode %d\n", ShortName, loop, Mode); //~VERBOSE
			// printf("[%s][%04d] -- Circular buffer: Determining next ready message and metadata starting from %d messages from circular buffer\n", ShortName, loop, RelativeMessageCounter); //~DEBUG
			printf("[%s][%04d] -- Circular buffer: Determining next ready message and metadata\n", ShortName, loop); //~DEBUG

			// Set the message counter
			lRelativeMessageCounter = 0; // Start with first slot
			// lRelativeMessageCounter = RelativeMessageCounter + 1;
			// lRelativeMessageCounter = RelativeMessageCounter; // Start with last message
			// if (RelativeMessageCounter + 1 > MessageSlots) lRelativeMessageCounter = 0; // Start with first message
			// else lRelativeMessageCounter = RelativeMessageCounter + 1; // Start with next message
			ec = 0; // Loop exit condition reset

			// Search for non empty messages
			// while (*(CircularBuffer + lRelativeMessageCounter * (MessageLength + npipes + oalignment) + moffset) == '\0') {
			while (lRelativeMessageCounter < MessageSlots) {
				if (lRelativeMessageCounter > MessageSlots) { if (ec > 0) return -1; lRelativeMessageCounter = 0; ec = 1; }
				// printf("[%s][%04d] -- Circular buffer: Testing message slot %d\n", ShortName, loop, lRelativeMessageCounter); //~DEBUG
				if (*(CircularBuffer + lRelativeMessageCounter * (MessageLength + npipes + oalignment) + moffset) == '\0') {
					lRelativeMessageCounter++;
					continue;
				}
				printf("[%s][%04d] -- Circular buffer: Next message found at slot %d\n", ShortName, loop, lRelativeMessageCounter + 1); //~DEBUG
				// memcpy(msg, CircularBuffer + lRelativeMessageCounter * (MessageLength + npipes + oalignment), MessageLength); // Copy whole memory segment

				// Determine if any pipe FDs are still available in metadata else delete message
				///TODO - implement function call instead
				// Reading message and metadata
				for (int i = 0; i < MessageSlots; i++) {
					for (int j = 0; j < sizeof(msg) + sizeof(mp) + sizeof(ms) + oalignment; j++) {
						if (i == lRelativeMessageCounter) {
							if (j < MessageLength) msg[j] = *(CircularBuffer + i * (MessageLength + npipes + oalignment) + j);
							if (j < MessageLength) Message[j] = *(CircularBuffer + i * (MessageLength + npipes + oalignment) + j);
							if ((j < MessageLength + alignment + sizeof(mp)) && (j > MessageLength + alignment)) mp[j] = *(CircularBuffer + i * (MessageLength + npipes + oalignment) + j);
							if ((j < MessageLength + alignment + sizeof(mp) + sizeof(ms)) && (j > MessageLength + alignment + sizeof(mp))) ms[j] = *(CircularBuffer + i * (MessageLength + npipes + oalignment) + j);
						}
						if (j == MessageLength) continue;
						if (j == MessageLength + alignment) continue;
					}
				}

				// Interpreting metadata
				// This is a static approach, it should be flexible and based upon the number of publishers and subscribers
				printf("[%s][%04d] -- Circular buffer: Message #%d: [ %s ]\n", ShortName, loop, lRelativeMessageCounter + 1, msg);
				printf("[%s][%04d] -- Circular buffer: Metadata: [ %c%c | %c%c%c ]\n", ShortName, loop, mp[0], mp[1], ms[0], ms[1], ms[2]);
				// printf("[%s][%04d] -- Circular buffer: Metadata: Publisher [ ", ShortName, loop);
				if (strcmp(mp, "PX") == 0) {
					*(SourcePipe) = **(Pipes);
					*(SourcePipe + 1) = 0;
				} else if (strcmp(mp, "XP") == 0) {
					*(SourcePipe) = 0;
					*(SourcePipe + 1) = **(Pipes + 1);
				} else return -1; // Error in case of recognizing a request message in buffer

				///TODO
				// Implement an iterated approach based on number of subscribers
				// Calculate size of array based on number of possible combinations or use a more sophisticated comparisson of strings
				// char x[32] = {'SSS', 'YSS', 'SYS', 'SSY', 'YYS', 'SYY', 'YSY', 'YYY'};
				// for (int i = 0; i < 8; i++) printf("%d\n", strcmp(x, "S"));
				// for (int i = 0; i < npipes; i++) for (int j = 0; j < 2; j++) printf("### ### Pipes[%d]: %d\n", i, *(Pipes + i)[j]);
				// for (int i = 0; i < npipes; i++) for (int j = 0; j < 2; j++) printf("### ### Pipes[%d][%d]: %d\n", i, j, *(Pipes + i)[j]);


				// Set all values to the pipes statically
				for (int i = 0; i < nSubscriber; i++) *(TargetPipe + i) = i * 4 + 10; // Initial values
				// All target pointers need to offset by n*2 + 1, so that writing pipe ends are addressed, but this can be addresed later by adding 1

				// if (strcmp(ms, "SSS") == 0) {
				// else if (strcmp(ms, "YSS") == 0) {
				if (strcmp(ms, "YSS") == 0) {
					*(TargetPipe) = 0;
				}
				else if (strcmp(ms, "SYS") == 0) {
					*(TargetPipe + 1) = 0;
				}
				else if (strcmp(ms, "SSY") == 0) {
					*(TargetPipe + 2) = 0;
				}
				else if (strcmp(ms, "YYS") == 0) {
					*(TargetPipe) = 0;
					*(TargetPipe + 1) = 0;
				}
				else if (strcmp(ms, "SYY") == 0) {
					*(TargetPipe + 1) = 0;
					*(TargetPipe + 2) = 0;
				}
				else if (strcmp(ms, "YSY") == 0) {
					*(TargetPipe) = 0;
					*(TargetPipe + 2) = 0;
				}
				else if (strcmp(ms, "YYY") == 0) {
					*(TargetPipe) = 0;
					*(TargetPipe + 1) = 0;
					*(TargetPipe + 2) = 0;
					DeleteMessage = lRelativeMessageCounter;
				}
				// printf("%d;%d;%d", *(TargetPipe), *(TargetPipe + 1), *(TargetPipe + 2));
				// printf(" ]\n", ShortName, loop);
				// printf("[%s][%04d] -- Circular buffer: Metadata both: [ %s | %s ]\n", ShortName, loop, mp, ms);
				// printf("[%s][%04d] -- Circular buffer: Metadata both: [ %c%c | %c%c%c ]\n", ShortName, loop, mp[0], mp[1], ms[0], ms[1], ms[2]);
				printf("[%s][%04d] -- Circular buffer: Metadata: [ %d;%d | %d;%d;%d ]\n", ShortName, loop, SourcePipe[0], SourcePipe[1], TargetPipe[0], TargetPipe[1], TargetPipe[2]);

				if (DeleteMessage >= 0) {
					printf("[%s][%04d] -- Circular buffer: Based on metadata message #%d to be deleted\n", ShortName, loop, lRelativeMessageCounter + 1); //~DEBUG
					mb = MediatorBuffer(Message, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, Pipes, DeleteMessage, 2);
					if (!mb) printf("[%s][%04d] -- Circular buffer: Based on metadata message #%d deleted\n", ShortName, loop, lRelativeMessageCounter + 1); //~DEBUG
				}

				// Determine the publisher in the same way using SourcePipe
				// Determine if the message is for the given subscriber
				for (int i = 0; i < nSubscriber; i++) {
					// printf("[%s][%04d] -- Circular buffer: Metadata checked for message #%d from pipe (FD %d): Subscriber condition %d == %d ]\n", ShortName, loop, lRelativeMessageCounter + 1, SourcePipe[i], PipeFD, TargetPipe[i] + 3); //~DEBUG
					printf("[%s][%04d] -- Circular buffer: Metadata checked for message #%d: Subscriber condition %d == %d ]\n", ShortName, loop, lRelativeMessageCounter + 1, PipeFD, TargetPipe[i]); //~DEBUG
					if (PipeFD == TargetPipe[i]) {
						printf("[%s][%04d] -- Circular buffer: Message #%d matches the subscriber pipe (FD %d)\n", ShortName, loop, lRelativeMessageCounter + 1, PipeFD);
						// Reconstruct the message
							// for (int i = 0; i < MessageLength; i++) msg[i] = *(Message + i);
							// msg[MessageLength] = '\0'; // "Zeroing" last character
						// Message = msg; //~DEBUG Doesn't work
						printf("[%s][%04d] -- Circular buffer: Message #%d [%s] loaded\n", ShortName, loop, lRelativeMessageCounter + 1, Message);
						// for (int i = 0; i < nSubscriber; i++) if (*(TargetPipe + i) != 0) {
						// printf("[%s][%04d] -- Circular buffer: PipeFD %d, TP %d\n", ShortName, loop, PipeFD, *(TargetPipe + i));
							PipeFD = *(TargetPipe + i); // Setting the target pipe FD
							// break;
						// }
						// printf("[%s][%04d] -- Circular buffer: PipeFD %d, TP %d, TP2 %d TP3 %d\n", ShortName, loop, PipeFD, *(TargetPipe), *(TargetPipe+1), *(TargetPipe+2));
						lc = 0;
						// return lRelativeMessageCounter;
						RelativeMessageCounter = lRelativeMessageCounter;
						return PipeFD;
						break;
					}
				}
			lRelativeMessageCounter++;
			} // Back to main loop seeking through all messages

			break;
		case 4:
		// Print circular buffers content
			// printf("[%s][%04d] -- Circular buffer: Mode %d\n", ShortName, loop, Mode); //~VERBOSE
			if (PipeFD >= 0) {
				if (*(CircularBuffer + PipeFD * (MessageLength + npipes + oalignment) + moffset) == '\0')
						printf("[%s][%04d] -- Circular buffer: Message #%d [    EMPTY     ]\n", ShortName, loop, PipeFD + 1);
				else {
					printf("[%s][%04d] -- Circular buffer: Message #%d [ ", ShortName, loop, PipeFD + 1);
					for (int j = 0; j < sizeof(msg) + sizeof(mp) + sizeof(ms) + oalignment; j++) {
						if (j == MessageLength) {
							printf(" | "); 
							continue;
						}
						if (j == MessageLength + alignment) continue;
						printf("%c", *(CircularBuffer + PipeFD * (MessageLength + npipes + oalignment) + j));
					}
					printf(" ]\n");
				}
			} else if (PipeFD == -1) {
			// Print all messages from circular buffer
				printf("[%s][%04d] -- Circular buffer: Buffer contents\n", ShortName, loop); //~DEBUG
				printf("[%s][%04d] -- Circular buffer: Message no [ Msg  |  Meta ]\n", ShortName, loop);
				me = 0; mf = 0;
				for (int i = 0; i < MessageSlots; i++) {
					if (*(CircularBuffer + i * (MessageLength + npipes + oalignment) + moffset) == '\0') {
						printf("[%s][%04d] -- Circular buffer: Message #%d [    EMPTY     ]\n", ShortName, loop, i + 1);
						me++;
						continue;
					}
					printf("[%s][%04d] -- Circular buffer: Message #%d [ ", ShortName, loop, i + 1);
					for (int j = 0; j < sizeof(msg) + sizeof(mp) + sizeof(ms) + oalignment; j++) {
						if (j == MessageLength) {
							printf(" | "); 
							continue;
						}
						if (j == MessageLength + alignment) continue;
						printf("%c", *(CircularBuffer + i * (MessageLength + npipes + oalignment) + j));
					}
					printf(" ]\n");
					mf++;
				}
				printf("[%s][%04d] -- Circular buffer: Buffer contents listed\n", ShortName, loop); //~DEBUG
				printf("[%s][%04d] -- Circular buffer: Messages: %d, empty message: %d, slots: %d\n", ShortName, loop, mf, me, MessageSlots); //~DEBUG
				if (mf != RelativeMessageCounter) {
					RelativeMessageCounter = mf;
					printf("[%s][%04d] -- Circular buffer: Update message counter: %d\n", ShortName, loop, RelativeMessageCounter); //~DEBUG
				}
			} else if (PipeFD == -2) {
			// Update message counter
				me = 0; mf = 0;
				for (int i = 0; i < MessageSlots; i++) {
						if (*(CircularBuffer + i * (MessageLength + npipes + oalignment) + moffset) == '\0') {
							me++;
							continue;
						}
					}
					mf++;
				// printf("[%s][%04d] -- Circular buffer: Messages: %d, empty message: %d, slots: %d\n", ShortName, loop, mf, me, MessageSlots); //~DEBUG
				if (mf != RelativeMessageCounter) {
					RelativeMessageCounter = mf;
					// printf("[%s][%04d] -- Circular buffer: Update message counter: %d\n", ShortName, loop, RelativeMessageCounter); //~DEBUG
				}
			}
			break;
		case 5:
		// Clear the circular buffer
			// printf("[%s][%04d] -- Circular buffer: Mode %d\n", ShortName, loop, Mode); //~VERBOSE
			if (PipeFD == 1) {
				// Print all messages before clearing operation
				MediatorBuffer(0, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, 0, -1, 4);
				printf("[%s][%04d] -- Circular buffer: Clearing the buffer\n", ShortName, loop); //~DEBUG
			}

			// Actual clearing operation
			// for (int i; i < (MessageBuffer); i++) *(CircularBuffer + i) = CHAR_MIN; // Manual solution
			memset(CircularBuffer, 0, MessageBuffer);

			if (PipeFD == 1) {
				// Print all messages after clearing operation
				MediatorBuffer(0, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, 0, -1, 4);
				printf("[%s][%04d] -- Circular buffer: Clearing of the buffer complete\n", ShortName, loop); //~DEBUG
			}
			RelativeMessageCounter = 0;
			break;
		case 6:
		// Replace message
		///TODO
		// Use PipeFD and RelativeMessageCounter
			// printf("[%s][%04d] -- Circular buffer: Mode %d\n", ShortName, loop, Mode); //~VERBOSE
			lRelativeMessageCounter = RelativeMessageCounter;
			printf("[%s][%04d] -- Circular buffer: Replacing message #%d with message #%d\n", ShortName, loop, lRelativeMessageCounter, PipeFD); //~DEBUG
			memset(CircularBuffer + PipeFD * (MessageLength + npipes + oalignment), 0, (MessageLength + npipes + alignment));
			break;
		case 7:
		// Reorder messages
		///TODO
		// Move free message slots to the end of the buffer
			// printf("[%s][%04d] -- Circular buffer: Mode %d\n", ShortName, loop, Mode); //~VERBOSE
			printf("[%s][%04d] -- Circular buffer: Reordering %d messages in the circular buffer\n", ShortName, loop, MessageSlots); //~DEBUG
			break;
		case 8:
		// Determine free message slot
			// printf("[%s][%04d] -- Circular buffer: Mode %d\n", ShortName, loop, Mode); //~VERBOSE
			printf("[%s][%04d] -- Circular buffer: Determine free message slot in the circular buffer\n", ShortName, loop); //~DEBUG

			// lRelativeMessageCounter = RelativeMessageCounter;
			lRelativeMessageCounter = 0;
			do {
				if (lRelativeMessageCounter > MessageSlots) { if (ec > 0) return -1; lRelativeMessageCounter = 0; ec = 1; }
				if (*(CircularBuffer + lRelativeMessageCounter * (MessageLength + npipes + oalignment) + moffset) == '\0') break;
				lRelativeMessageCounter++;
			} while (*(CircularBuffer + lRelativeMessageCounter * (MessageLength + npipes + oalignment) + moffset) != '\0');
			// while (*(CircularBuffer + lRelativeMessageCounter * (MessageLength + npipes + oalignment)) != '\0') {
				// lRelativeMessageCounter++;
				// if (lRelativeMessageCounter > MessageSlots) { if (ec > 0) return -1; lRelativeMessageCounter = 0; ec = 1; }
			// }
			printf("[%s][%04d] -- Circular buffer: Found free message #%d slot in the circular buffer\n", ShortName, loop, lRelativeMessageCounter + 1); //~DEBUG
			return lRelativeMessageCounter;
			break;
		case 9:
		// Update metadata for message
			lRelativeMessageCounter = RelativeMessageCounter; // Start with first slot
			// printf("[%s][%04d] -- Circular buffer: Mode %d\n", ShortName, loop, Mode); //~VERBOSE
			printf("[%s][%04d] -- Circular buffer: Updating metadata for message %d\n", ShortName, loop, PipeFD); //~DEBUG

			// Reading metadata
			for (int i = 0; i < MessageSlots; i++) {
				// if (*(CircularBuffer + i * (MessageLength + npipes + oalignment)) == '\0') {
					// printf("[%s][%04d] -- Circular buffer: Message #%d [    EMPTY     ]\n", ShortName, loop, i + 1);
				// }
				for (int j = 0; j < sizeof(msg) + sizeof(mp) + sizeof(ms) + oalignment; j++) {
					if (i == lRelativeMessageCounter) {
						if (j < MessageLength) msg[j] = *(CircularBuffer + i * (MessageLength + npipes + oalignment) + j);
						if (j < MessageLength) Message[j] = *(CircularBuffer + i * (MessageLength + npipes + oalignment) + j);
						if ((j < MessageLength + alignment + sizeof(mp)) && (j > MessageLength + alignment)) mp[j] = *(CircularBuffer + i * (MessageLength + npipes + oalignment) + j);
						if ((j < MessageLength + alignment + sizeof(mp) + sizeof(ms)) && (j > MessageLength + alignment + sizeof(mp))) ms[j] = *(CircularBuffer + i * (MessageLength + npipes + oalignment) + j);
					}
					if (j == MessageLength) continue;
					if (j == MessageLength + alignment) continue;
				}
			}

			printf("[%s][%04d] -- Circular buffer: Original metadata read: [ %c%c | %c%c%c ]\n", ShortName, loop, mp[0], mp[1], ms[0], ms[1], ms[2]);

			// Interpreting metadata
			// Publisher metadata not modified
			// Process metadata for subscribers
			for (int i = 0; i < nSubscriber; i++) *(TargetPipe + i) = i * 4 + 10; // Set all values to the pipes statically

			// if ((PipeFD == *(TargetPipe)) && (!strcmp(ms, "YSS") == 0)) *(TargetPipe) = 0;
			// else if ((PipeFD == *(TargetPipe + 1)) && (!strcmp(ms, "SYS") == 0)) *(TargetPipe + 1) = 0;
			// for (int i = 0; i < nSubscriber; i++) if ((PipeFD == *(TargetPipe + i)) && ((!strcmp(ms, "SSS") == 0) || (!strcmp(ms, "YSS") == 0) || (!strcmp(ms, "YYS") == 0) || (!strcmp(ms, "SYY") == 0) || (!strcmp(ms, "YSY") == 0) || (!strcmp(ms, "SSY") == 0)) *(TargetPipe + i) = 0;
			// for (int i = 0; i < nSubscriber; i++) if (PipeFD == *(TargetPipe + i)) *(TargetPipe + i) = 0;
			// for (int i = 0; i < nSubscriber; i++) if (*(TargetPipe + i) == 0) ms[i] = 'Y';
			for (int i = 0; i < nSubscriber; i++) if (PipeFD == *(TargetPipe + i)) ms[i] = 'Y';

			// printf("%d;%d;%d\n", *(TargetPipe), *(TargetPipe + 1), *(TargetPipe + 2));//~DEBUG
			// printf("%c;%c;%c\n", *(ms + 0), *(ms + 1), *(ms + 2));//~DEBUG
			// if (((PipeFD == *(TargetPipe)) || (PipeFD == *(TargetPipe + 1)) || (PipeFD == *(TargetPipe + 2))) && (!strcmp(ms, "YYY") == 0)) {
			// if (!strcmp(ms, "YYY") == 0) {
				// *(TargetPipe) = 0;
				// *(TargetPipe + 1) = 0;
				// *(TargetPipe + 2) = 0;
			if (!strcmp(ms, "YYY") == 0) DeleteMessage = lRelativeMessageCounter; else DeleteMessage = -1; // Check delete message condition

			printf("[%s][%04d] -- Circular buffer: Modified metadata: [ %c%c | %c%c%c ]\n", ShortName, loop, mp[0], mp[1], ms[0], ms[1], ms[2]);

		// /* Store metadata in char in an inefficient manner, so each bytes serves as storage for only 1 information
			// for (int i = 0; i < nSubscriber; i++) ms[i] = CHAR_MAX;
			// for (int i = 0; i < nSubscriber; i++) ms[i] = 'S';
			ms[nSubscriber] = '\0';
			if (DeleteMessage >= 0) {
				printf("[%s][%04d] -- Circular buffer: Based on metadata message #%d to be deleted\n", ShortName, loop, DeleteMessage);
				mb = MediatorBuffer(Message, MessageLength, CircularBuffer, MessageBuffer, RelativeMessageCounter, nPublisher, nSubscriber, Pipes, DeleteMessage, 2);
				return RelativeMessageCounter;
				printf("[%s][%04d] -- Circular buffer: Saved metadata: [ %c%c | %c%c%c ]\n", ShortName, loop, mp[0], mp[1], ms[0], ms[1], ms[2]);
			} else {
				printf("[%s][%04d] -- Circular buffer: Writing message #%d metadata to slot #%d to circular buffer\n", ShortName, loop, msg, lRelativeMessageCounter); //~DEBUG
				// for (int i = 0; i < MessageLength; i++) { *(CircularBuffer + i + offset) = *(Message + i);} // Actual writing operation
				// for (int i = 0; i < (sizeof(mp)); i++) { *(CircularBuffer + i + offset + sizeof(msg) + oalignment) = mp[i];}
				for (int i = 0; i < (sizeof(ms)); i++) { *(CircularBuffer + i + offset + sizeof(msg) + sizeof(mp) + oalignment) = ms[i];}
				return 0;
			}
			return -1;
			break;
	}
	ec = 0;
	return 0; // Returns pipe FD for the writing operation, otherwise 0 if success for other operations
}
