ES4268738-AaRP-AS02A-M.cpp                                                                          0000644 0001750 0001750 00000135106 12656157455 013746  0                                                                                                    ustar   emeres                          emeres                                                                                                                                                                                                                 /*
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
                                                                                                                                                                                                                                                                                                                                                                                                                                                          ES4268738-AaRP-AS02A-P1.cpp                                                                         0000644 0001750 0001750 00000013430 12656157455 014025  0                                                                                                    ustar   emeres                          emeres                                                                                                                                                                                                                 /*
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
                                                                                                                                                                                                                                        ES4268738-AaRP-AS02A-P2.cpp                                                                         0000644 0001750 0001750 00000013430 12656157455 014026  0                                                                                                    ustar   emeres                          emeres                                                                                                                                                                                                                 /*
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
                                                                                                                                                                                                                                        ES4268738-AaRP-AS02A-S1.cpp                                                                         0000644 0001750 0001750 00000014704 12656157455 014035  0                                                                                                    ustar   emeres                          emeres                                                                                                                                                                                                                 /*
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
                                                            ES4268738-AaRP-AS02A-S2.cpp                                                                         0000644 0001750 0001750 00000014704 12656157455 014036  0                                                                                                    ustar   emeres                          emeres                                                                                                                                                                                                                 /*
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
                                                            ES4268738-AaRP-AS02A-S3.cpp                                                                         0000644 0001750 0001750 00000014704 12656157455 014037  0                                                                                                    ustar   emeres                          emeres                                                                                                                                                                                                                 /*
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
                                                            ES4268738-AaRP-AS02B.cpp                                                                            0000644 0001750 0001750 00000102276 12656456045 013554  0                                                                                                    ustar   emeres                          emeres                                                                                                                                                                                                                 /*
Assignment 02 - Advanced and Robot Programming
Publisher, subscriber and mediator scheme in POSIX environment using pthread
Ernest Skrzypczyk -- 4268738 -- EMARO // ES4268738-AaRP-AS02
*/
// Pthread
// Compile with:
//	g++ -Wall -o AS02B ES4268738-AaRP-AS02B.cpp
#include "ES4268738-AaRP-AS02.h"
#include <fcntl.h> //semaphore
#include <inttypes.h> //uint64
#include <iostream>
#include <cmath>
#include <pthread.h> //pthread functions
#include <signal.h>
#include <semaphore.h> //semaphore
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

/* Differences between the pipe and pthread version:
 * Instead of fork() use pthread_create()
 * Instead of waitpid() use pthread_join()
 * Usage of pthread_mutex_lock() and pthread_mutex_unlock() for synchronization
 * The code base largely stays the same
 * Instead of pipes use shared memory
 * Define global variables for access to all threads
// */


// Global variables
// Name arrays
char Names[][20] = {{"Init"}, {"Mediator"}, {"Publisher-1"}, {"Publisher-2"}, {"Subscriber-1"}, {"Subscriber-2"}, {"Subscriber-3"}};
// Exchange the commenting in the next two lines to switch between GNU/Linux console color codes
// char ShortNames[][5] = {{"INIT"}, {"MEDI"}, {"PUB1"}, {"PUB2"}, {"SUB1"}, {"SUB2"}, {"SUB3"}}; //~COLOR
char ShortNames[][20] = {{CI"INIT"CR}, {CM"MEDI"CR}, {CP1"PUB1"CR}, {CP2"PUB2"CR}, {CS1"SUB1"CR}, {CS2"SUB2"CR}, {CS3"SUB3"CR}}; //~COLOR

// Component condition variables
int ConditionPub1 = 1, ConditionPub2 = 1, ConditionMedi = 1, ConditionSub1 = 1, ConditionSub2 = 1, ConditionSub3 = 1;

//Mutexes
pthread_mutex_t mutexMessage = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexSubscriber1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexSubscriber2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexSubscriber3 = PTHREAD_MUTEX_INITIALIZER;

// /* Signal handler //~SEMAPHORE
void SignalHandler(int SignalNumber) {
	// printf("[%s][%04d][SIGNAL HANDLER] Process %d received signal: %d\n", ShortName, loop, getpid(), SignalNumber);
	printf("[SIGNAL HANDLER] Process %d received signal: %d\n", getpid(), SignalNumber);
	ConditionPub1 = 0;
	ConditionPub2 = 0;
	ConditionMedi = 0;
	ConditionSub1 = 0;
	ConditionSub2 = 0;
	ConditionSub3 = 0;
	// Unlocking all mutexes to prevent hanging threads
	pthread_mutex_unlock(&mutexMessage);
	pthread_mutex_unlock(&mutexSubscriber1);
	pthread_mutex_unlock(&mutexSubscriber2);
	pthread_mutex_unlock(&mutexSubscriber3);
	// exit(SignalNumber); //~
}
// */

// Messages
// Assume uniform message lengths, but leave the possibility for changes in the future
int MessageLength = MSG_LENGTH;
// int MessageLengths[] = {MEDI, PUB1, PUB2, SUB1, SUB2, SUB3};
// int MessageLengths[ncomponents] = {MessageLength, MessageLength, MessageLength, MessageLength, MessageLength, MessageLength}; 
// One message for input, nSubscriber messages for output
char MessagePublisher1[MSG_LENGTH];
char MessagePublisher2[MSG_LENGTH];
char MessageSubscriber1[MSG_LENGTH];
char MessageSubscriber2[MSG_LENGTH];
char MessageSubscriber3[MSG_LENGTH];
// Mediator relevant
int RequestGranted = 0;
int NewMessage = 0;
int NewRequest = 0;

// Buffers
// int MessageBuffer = MessageLength * 8;
// int MessageBuffer = MessageLength * tpratio;
int MessageBuffer = 8; // In case of vector for circular buffer

int nPublisher; int nSubscriber;
// int MessageLength; int MessageBuffer; char Message;
char AlphabetUpper; char AlphabetLower; char Digits;

//Threads functions declarations
void *mediator(void *ptr);
void *publisher1(void *ptr);
void *publisher2(void *ptr);
void *subscriber1(void *ptr);
void *subscriber2(void *ptr);
void *subscriber3(void *ptr);

// Function definitions
/* Random character generation from given charset -- Used inside publisher
void GenerateRandomCharacter(char *charset, int s, int l, char *message) {
	int t; char c[l];

	for (int i = 0; i < l; ++i) {
		t = (rand() * getpid()) % s;
		if (t < 0) t *= -1; // Make sure the pseudo random number is positive
		c[i] = charset[t];
	}
	for (int i = 0; i < l; ++i) message[i] = c[i]; // Assign generated values one by one
}
// */

// Determine pthread identificator
uint64_t gettid() {
	pthread_t ptid = pthread_self();
	uint64_t threadId = 0;
	memcpy(&threadId, &ptid, std::min(sizeof(threadId), sizeof(ptid)));
	return threadId;
}

// Decode metadata
int decodemetadata(int metadata, int nbits, int wbit) {
	int cbit[nbits];
	for (int i = 0; i < nbits; i++) {
		if (metadata / pow(2, i)) {
			metadata = metadata - pow(2, i);
			cbit[i] = 1;
		}
		if (wbit == i) return cbit[i];
	}
	return 0;
}

// Main function of the init process
int main()
{

// Register signal handler
signal(SIGUSR1, SignalHandler);

// PThread holders
int pid; // Init process identificator
int nPublisher = 2;
int nSubscriber = 3;
// int npipes = nPublisher + 2 * nSubscriber; // Number of pipes
int npipes = nPublisher + nSubscriber * 2; // Number of pipes // Obsolete but kept for simplicity of adjusting the source code
int ncomponents = nPublisher + nSubscriber + 1;
int nthreads = ncomponents;
pthread_t PThreads[nthreads]; // 6 threads
int ExitStatus[nthreads], status;

// /* Timings - Defined in the header
int TimeParameters[ncomponents + 1] = {MEDI_DECAY, MEDI_PERIOD, PUB1_PERIOD, PUB2_PERIOD, SUB1_PERIOD, SUB2_PERIOD, SUB3_PERIOD};
int TimePeriods[ncomponents + 1] = {MEDI_DECAY, MEDI_PERIOD, PUB1_PERIOD, PUB2_PERIOD, SUB1_PERIOD, SUB2_PERIOD, SUB3_PERIOD};
int tpmin = (-1) * TimeParameters[0]; int tpmax = 0;
for (int i = 2; i < ncomponents + 1; i++) {
	if (tpmax < TimeParameters[i]) tpmax = TimeParameters[i];
	if (tpmin < (-1) * TimeParameters[i]) tpmin = (-1) * TimeParameters[i];
}
int tpratio = tpmax / ((-1) * tpmin); // Should be the actual ratio of slowest subscriber to fastest publisher
// */

pid = getpid(); // Init PID
printf("[%s][INFO] PID %d: Start %s\n", ShortNames[0], pid, Names[0]);

//Publishers character sets -- access to all publishers
// Alphabet definition - Upper characters // String literal, could be also char *AlphabetUpper = "..."
char AlphabetUpper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Alphabet definition - Lower characters
char AlphabetLower[] = "abcdefghijklmnopqrstuvwxyz";

// Digits definition
char Digits[] = "0123456789";

// Init
printf("[%s][INFO] -- Message length: %d\n", ShortNames[0], MessageLength);

// Creating pthreads
// PThreads[1] -- Mediator
// PThreads[2] -- Publisher 1
// PThreads[3] -- Publisher 2
// PThreads[4] -- Subscriber 1
// PThreads[5] -- Subscriber 2
// PThreads[6] -- Subscriber 3

for (int i = 1; i <= nthreads; i++) { // Loop for error checking and calling all of the components
	printf("[%s][%s] Creating thread: %s @PID %d\n", ShortNames[0], ShortNames[i], Names[i], getpid());

	switch (i) {
		case 1:
			ExitStatus[i] = pthread_create(&PThreads[i], NULL, mediator, NULL);
			if (ExitStatus[i]) {
				perror("pthread_create()");
				printf("[%s][ERROR] PID %d pthread_create() error: %d\n", ShortNames[i], gettid(), PThreads[i]);
			}
			break;
		case 2:
			ExitStatus[i] = pthread_create(&PThreads[i], NULL, publisher1, NULL);
			if (ExitStatus[i]) {
				perror("pthread_create()");
				printf("[%s][ERROR] PID %d pthread_create() error: %d\n", ShortNames[i], gettid(), PThreads[i]);
			}
			break;
		case 3:
			ExitStatus[i] = pthread_create(&PThreads[i], NULL, publisher2, NULL);
			if (ExitStatus[i]) {
				perror("pthread_create()");
				printf("[%s][ERROR] PID %d pthread_create() error: %d\n", ShortNames[i], gettid(), PThreads[i]);
			}
			break;
		case 4:
			ExitStatus[i] = pthread_create(&PThreads[i], NULL, subscriber1, NULL);
			if (ExitStatus[i]) {
				perror("pthread_create()");
				printf("[%s][ERROR] PID %d pthread_create() error: %d\n", ShortNames[i], gettid(), PThreads[i]);
			}
			break;
		case 5:
			ExitStatus[i] = pthread_create(&PThreads[i], NULL, subscriber2, NULL);
			if (ExitStatus[i]) {
				perror("pthread_create()");
				printf("[%s][ERROR] PID %d pthread_create() error: %d\n", ShortNames[i], gettid(), PThreads[i]);
			}
			break;
		case 6:
			ExitStatus[i] = pthread_create(&PThreads[i], NULL, subscriber3, NULL);
			if (ExitStatus[i]) {
				perror("pthread_create()");
				printf("[%s][ERROR] PID %d pthread_create() error: %d\n", ShortNames[i], gettid(), PThreads[i]);
			}
			break;
	}
	// exit(ExitStatus[i - 1]);
}

// /* Exit codes handling
	// sleep(1); // Delay for exit monitoring

	// Init
	for (int i = 1; i <= nthreads; i++) { // Loop for waiting for all of the threads to end
		ExitStatus[i] = pthread_join(PThreads[i], NULL); // Pid could be also -1 or 0 in this case
		if (ExitStatus[i] < 0) {
			perror("pthread_join()");
			printf("[%s][ERROR] PID %d pthread_join() error: %d\n", ShortNames[i], PThreads[i - 1], ExitStatus[i]); // WEXITSTATUS macro should usually be only called if WIFEXITED(status) returns 1, so not in an error case; Debug purpose only
			// return(1); // Exit the process with an error
		}
	// printf("Iteration: %d\n", i); //~DEBUG
	}

	for (int i = 1; i <= nthreads; i++) { // Show exit status for each component
		// printf("[%s][%s] Thread %d exit status: %d\n", ShortNames[0], ShortNames[i + 1], (int*) PThreads[i], ExitStatus[i]);
		printf("[%s][%s] Thread %d exit status: %d\n", ShortNames[0], ShortNames[i], i, ExitStatus[i]);
	}

	// printf("[%s][INFO] Thread %d: Exiting %s\n", ShortNames[0], (int*) PThreads[0], Names[0]);
	printf("[%s][INFO] PID %d: Exiting %s\n", ShortNames[0], pid, Names[0]);
// */

// return 0;
exit(EXIT_SUCCESS);
}

// Pthreads start routine definitions
// Mediator
void *mediator(void *ptr) {
// Determine pthread identificator
uint64_t ptid = gettid();

// Default values
// char ShortName[20] = "MEDI"; // The size is necessary for the GNU/Linux console codes //~COLOR
char ShortName[20] = CM"MEDI"CR; // The size is necessary for the GNU/Linux console codes //~COLOR
int TimePeriod = MEDI_PERIOD;

// Component specific parameters
int loop = 1;

// sem_t *semaphore0 = sem_open("semaphoremediator", O_CREAT|O_EXCL, 0, 1);
// sem_unlink("semaphoremediator"); // Prevent a stray semaphore

printf("[%s][%04d] -- %s initialization\n", ShortName, loop, Names[1]);
printf("[%s][INFO] -- Pthread ID: %d\n", ShortName, ptid);
printf("[%s][INFO] -- Time period: %d\n", ShortName, TimePeriod);

// Clear mediator circular buffer
// std::vector<char> CircularBuffer;
// std::vector<std::string> CircularBuffer;
// std::vector<char*> CircularBuffer;
// std::vector<int> MetadataBuffer;
char CircularBuffer[MessageBuffer * MessageLength];
// memset(CircularBuffer, 0, MessageBuffer);
// CircularBuffer.reserve(MessageBuffer);
// CircularBuffer.clear();
// MetadataBuffer.reserve(MessageBuffer);
// MetadataBuffer.clear();
// int MetadataBuffer[MessageBuffer * nSubscriber];
int MetadataBuffer[MessageBuffer];

int MessageCounter = 0;
int mt = 0;

// Initial delay
usleep(TimePeriod * 1000); // usleep used for emulating msleep
// usleep(TimePeriod * ptid); // usleep used for emulating msleep
// usleep(TimePeriod * ptid); // usleep used for emulating msleep

char msg[MessageLength]; // Local copy of message
char smsg[MessageLength]; // Local copy of message
// std::string msg[MessageLength]; // Local copy of message

// Main loop
do {

if (NewMessage) {
// while (NewMessage) {

// for (unsigned int i = 0; i < CircularBuffer.size(); i++) printf("[%s][%04d] -- [ %s ]\n", ShortName, loop, CircularBuffer[i]);

	// /* Check if the circular buffer is full
	// if (MessageCounter >= MessageBuffer) {
	if (MessageCounter >= 2) {
		printf("[%s][%04d] -- Circular buffer full, capacity %d\n", ShortName, loop, MessageBuffer);
		printf("[%s][%04d] -- Circular and metadata buffers content\n", ShortName, loop);
		// for (int i = 0; i < MessageBuffer; i++) printf("[%s][%04d] -- [ %s ][ %d ]\n", ShortName, loop, CircularBuffer[i], MetadataBuffer[i]);
		// memset(CircularBuffer, 0, MessageLength); // Delete message from shared memory
		// memset(MetadataBuffer, 0, 1); // Delete message from shared memory
		// CircularBuffer.clear(); // Clear whole buffer
		// MetadataBuffer.clear(); // Clear whole buffer
		// MessageCounter = 0;
		// CircularBuffer.erase(CircularBuffer.begin()); // Delete first element
		// CircularBuffer.pop_back(); // Delete first element
		// MetadataBuffer.pop_back(); // Delete first element
		MessageCounter--;
		printf("[%s][%04d] -- Circular and metadata buffers content\n", ShortName, loop);
		// for (int i = 0; i < MessageLength; i++) sprintf(smsg, "%c", *(CircularBuffer + i));
		// for (int i = 0; i < MessageLength; i++) sprintf(smsg, "%c", CircularBuffer[i]);
		// for (int i = 0; i < MessageLength; i++) sprintf(smsg, "%c", msg[i]);
		int offset = 0;
		for (int i = 0; i < MessageLength; i++) { *(CircularBuffer + i + offset) = *(msg + i);}
		// for (int i = 0; i < MessageLength; i++) msg[i] = *(Message + i); // Reconstructing the message
		// for (int i = 0; i < MessageBuffer; i++) printf("[%s][%04d] -- [ %s ][ %d ]\n", ShortName, loop, *(CircularBuffer + i * MessageLength), *(MetadataBuffer + i));
		for (int i = 0; i < MessageBuffer; i++) printf("[%s][%04d] -- [ %s ][ %d ]\n", ShortName, loop, smsg, *(MetadataBuffer + i));
	}
	// */

	printf("[%s][%04d] -- Publisher messages %d before processing\n", ShortName, loop, NewMessage);
	if (NewMessage == 1) {
	// if (!(NewMessage % 1)) {
		printf("[%s][%04d] -- Locking mutex\n", ShortName, loop);
		pthread_mutex_lock(&mutexMessage); // Lock mutex on Message
		printf("[%s][%04d] -- Reading %d bytes from shared memory\n", ShortName, loop, MessageLength);

		memcpy(msg, MessagePublisher1, MessageLength); // Read from shared memory
		memset(MessagePublisher1, 0, MessageLength); // Delete message from shared memory
		NewMessage = 0; // Clear new message indicator
		// NewMessage = NewMessage - 1; // Clear new message indicator for publisher 1

		printf("[%s][%04d] -- %d bytes read from shared memory\n", ShortName, loop, sizeof(msg));
		pthread_mutex_unlock(&mutexMessage); // Unlock mutex on Message
		printf("[%s][%04d] -- Unlocking mutex\n", ShortName, loop);
		printf("[%s][%04d] -- Reading message from publisher: [%s]\n", ShortName, loop, msg);
		// msg[MessageLength] = '\0'; //DB
		// printf("[%s][%04d] -- Reading message from publisher: [%s]\n", ShortName, loop, msg); //DB
	}

	if (NewMessage == 2) {
	// if (!(NewMessage % 2)) {
		printf("[%s][%04d] -- Locking mutex\n", ShortName, loop);
		pthread_mutex_lock(&mutexMessage); // Lock mutex on Message
		printf("[%s][%04d] -- Reading %d bytes from shared memory\n", ShortName, loop, MessageLength);

		memcpy(msg, MessagePublisher2, MessageLength); // Read from shared memory
		memset(MessagePublisher2, 0, MessageLength); // Delete message from shared memory
		NewMessage = 0; // Clear new message indicator
		// NewMessage = NewMessage - 2; // Clear new message indicator for publisher 1

		printf("[%s][%04d] -- %d bytes read from shared memory\n", ShortName, loop, sizeof(msg));
		pthread_mutex_unlock(&mutexMessage); // Unlock mutex on Message
		printf("[%s][%04d] -- Unlocking mutex\n", ShortName, loop);
		printf("[%s][%04d] -- Reading message from publisher: [%s]\n", ShortName, loop, msg);
		// msg[MessageLength] = '\0'; //DB
		// printf("[%s][%04d] -- Reading message from publisher: [%s]\n", ShortName, loop, msg); //DB
	}

	// if (strcmp(msg, "\0")) {
	if (*(msg) != '\0') {
	// if (strcmp(msg, '\0')) {
		// Processing message
		printf("[%s][%04d] -- Sending message [%s] to buffer\n", ShortName, loop, msg);
		
		// Send message to buffer
		///TODO
		// CircularBuffer.push_back(msg.c_cstr());
		// smsg = *(msg);
		// CircularBuffer.push_back(msg);
		// sprintf(smsg, "%d", MessageCounter); // Contruct a message with metadata
		// CircularBuffer.push_back(smsg);
		// CircularBuffer.push_back("ASDC");
		// MetadataBuffer.push_back(7); // 2^0 + 2^1 + 2^2 = 1 + 2 + 4 = 7
		// MetadataBuffer.push_back(MessageCounter); // 2^0 + 2^1 + 2^2 = 1 + 2 + 4 = 7
		*(CircularBuffer + MessageCounter * MessageLength) = *(msg);
		*(MetadataBuffer + MessageCounter) = 7;
		// printf("[%s][%04d] -- Message [%s] stored in circular buffer\n", ShortName, loop, CircularBuffer(CircularBuffer.end()));
		MessageCounter++;
	}
	printf("[%s][%04d] -- Publisher messages %d after processing\n", ShortName, loop, NewMessage);
	// memset(msg, 0, MessageLength); // Delete message from shared memory
}

if (NewRequest) {
// while (NewRequest) {
	printf("[%s][%04d] -- Subscriber requests %d before processing\n", ShortName, loop, NewRequest);
	// if (!strcmp(msg, "REQ1")) {
	if (NewRequest == 1) {
	// if (!(NewRequest % 1)) {
		// Subscriber 1
		printf("[%s][%04d] -- Serving subscriber 1\n", ShortName, loop);

		// Get message from buffer
		///TODO
		memset(msg, 0, MessageLength + 1); // Delete message copied from shared memory
		memcpy(msg, "SER1", MessageLength);

		// Sending message to subscriber
		// printf("[%s][%04d] -- Locking mutex for subscriber 1\n", ShortName, loop);
		// pthread_mutex_lock(&mutexSubscriber1); // Lock mutex on Message
		printf("[%s][%04d] -- Writing %d bytes to shared memory\n", ShortName, loop, MessageLength);

		memcpy(MessageSubscriber1, msg, MessageLength); // Write to shared memory

		printf("[%s][%04d] -- %d bytes written to shared memory\n", ShortName, loop, sizeof(msg));
		RequestGranted = 1;
		printf("[%s][%04d] -- Granting request %d\n", ShortName, loop, RequestGranted);
		// Update request indicator
		NewRequest = 0;
		// NewRequest = NewRequest - 1;

		pthread_mutex_unlock(&mutexSubscriber1); // Unlock mutex on Message
		printf("[%s][%04d] -- Unlocking mutex for subscriber 1\n", ShortName, loop);
		// Another mutex could help make sure the transaction was conducted

		// Update tags on message
		///TODO

	// } else if (!strcmp(msg, "REQ2")) {
	}
	if (NewRequest == 2) {
	// if (!NewRequest % 2) {
		// Subscriber 2
		printf("[%s][%04d] -- Serving subscriber 2\n", ShortName, loop);

		// Get message from buffer
		///TODO
		memset(msg, 0, MessageLength + 1); // Delete message copied from shared memory
		memcpy(msg, "SER2", MessageLength);

		// Sending message to subscriber
		// printf("[%s][%04d] -- Locking mutex for subscriber 1\n", ShortName, loop);
		// pthread_mutex_lock(&mutexSubscriber1); // Lock mutex on Message
		printf("[%s][%04d] -- Writing %d bytes to shared memory\n", ShortName, loop, MessageLength);

		memcpy(MessageSubscriber2, msg, MessageLength); // Write to shared memory

		printf("[%s][%04d] -- %d bytes written to shared memory\n", ShortName, loop, sizeof(msg));
		RequestGranted = 2;
		printf("[%s][%04d] -- Granting request %d\n", ShortName, loop, RequestGranted);
		// Update request indicator
		NewRequest = 0;
		// NewRequest = NewRequest - 2;

		pthread_mutex_unlock(&mutexSubscriber2); // Unlock mutex on Message
		printf("[%s][%04d] -- Unlocking mutex for subscriber 2\n", ShortName, loop);
		// Another mutex could help make sure the transaction was conducted

		// Update tags on message
		///TODO
		
	// } else if (!strcmp(msg, "REQ3")) {
	}
	if (NewRequest == 3) {
	// if (!NewRequest % 4) {
		// Subscriber 3
		printf("[%s][%04d] -- Serving subscriber 3\n", ShortName, loop);

		// Get message from buffer
		///TODO
		memset(msg, 0, MessageLength + 1); // Delete message copied from shared memory
		memcpy(msg, "SER3", MessageLength);

		// Sending message to subscriber
		// printf("[%s][%04d] -- Locking mutex for subscriber 3\n", ShortName, loop);
		// pthread_mutex_lock(&mutexSubscriber1); // Lock mutex on Message
		printf("[%s][%04d] -- Writing %d bytes to shared memory\n", ShortName, loop, MessageLength);

		memcpy(MessageSubscriber3, msg, MessageLength); // Write to shared memory

		printf("[%s][%04d] -- %d bytes written to shared memory\n", ShortName, loop, sizeof(msg));
		RequestGranted = 3;
		printf("[%s][%04d] -- Granting request %d\n", ShortName, loop, RequestGranted);
		// Update request indicator
		NewRequest = 0;
		// NewRequest = NewRequest - 1;

		pthread_mutex_unlock(&mutexSubscriber3); // Unlock mutex on Message
		printf("[%s][%04d] -- Unlocking mutex for subscriber 3\n", ShortName, loop);
		// Another mutex could help make sure the transaction was conducted

		// Update tags on message
		///TODO
		// mt = MetadataBuffer(MetadataBuffer.begin());
		// decodemetadata(, )
	}
	memset(msg, 0, MessageLength); // Delete message from shared memory
	printf("[%s][%04d] -- Subscriber requests %d after processing\n", ShortName, loop, NewRequest);
}

// printf("[%s][%04d] -- Suspending mediator\n", ShortName, loop);
// printf("[%s][%04d] -- Suspending mediator for %d ms\n", ShortName, loop, TimePeriod); //VB
// Assume zero delay in producing and transmitting data
// TimePeriod pseudo corresponds to the period of the component
usleep(TimePeriod * 1000); // usleep used for emulating msleep

loop++; // Increase loop counter each cycle
} while (ConditionMedi);

// sem_post(semaphore0); //~SEMAPHORE // Releasing semaphore for stuck zombie clients

// Exit message after receiving signal from signal handler
printf("[%s][%04d] Thread id %d: Exiting %s\n", ShortName, loop, ptid, Names[1]);

}

// Publisher 1
void *publisher1(void *ptr) {
// Determine pthread identificator
uint64_t ptid = gettid();

// Default values
// char ShortName[20] = "PUB1"; // The size is necessary for the GNU/Linux console codes //~COLOR
char ShortName[20] = CP1"PUB1"CR; // The size is necessary for the GNU/Linux console codes //~COLOR
int TimePeriod = PUB1_PERIOD;

// Component specific parameters
int loop = 1;

// Character set
// Alphabet definition - Upper characters
char AlphabetUpper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

printf("[%s][%04d] -- %s initialization\n", ShortName, loop, Names[2]);
printf("[%s][INFO] -- Pthread ID: %d\n", ShortName, ptid);
printf("[%s][INFO] -- Time period: %d\n", ShortName, TimePeriod);

// Initial delay
usleep(TimePeriod * 1000); // usleep used for emulating msleep
// usleep(TimePeriod * ptid); // usleep used for emulating msleep

char msg[MessageLength]; // Local copy of message

// Main loop
do {
// Generate random character string and write the generated message to the shared memory
for (int i = 0; i < MessageLength;) {
	int t;
	t = (rand() * getpid()) % sizeof(AlphabetUpper);
	if (t < 0) t *= -1; // Make sure the pseudo random number is positive
	msg[i] = AlphabetUpper[t];
	if (msg[i] != '\0') i++;
}
printf("[%s][%04d] -- Publishing message: [%s]\n", ShortName, loop, msg);

// if (NewMessage) usleep(MEDI_PERIOD * 1000);

printf("[%s][%04d] -- Locking mutex\n", ShortName, loop);
pthread_mutex_lock(&mutexMessage); // Lock mutex on Message
printf("[%s][%04d] -- Writing %d bytes to shared memory\n", ShortName, loop, MessageLength);

	memcpy(MessagePublisher1, msg, MessageLength); // Write to shared memory
	NewMessage = 1;

printf("[%s][%04d] -- %d bytes written to shared memory\n", ShortName, loop, sizeof(MessagePublisher1));
// printf("[%s][%04d] -- %d bytes written to shared memory\n", ShortName, loop, MessageLength);
pthread_mutex_unlock(&mutexMessage); // Unlock mutex on Message
printf("[%s][%04d] -- Unlocking mutex\n", ShortName, loop);

// Assume zero delay in producing and transmitting data
// TimePeriod pseudo corresponds to the period of the component
printf("[%s][%04d] -- Suspending publisher 1 for %d ms\n", ShortName, loop, TimePeriod);
usleep(TimePeriod * 1000); // usleep used for emulating msleep

loop++; // Increase loop counter each cycle
} while (ConditionPub1);

// Exit message after receiving signal from signal handler
printf("[%s][%04d] Thread id %d: Exiting %s\n", ShortName, loop, ptid, Names[2]);

	pthread_exit(0);
}

// Publisher 2
void *publisher2(void *ptr) {
// Determine pthread identificator
uint64_t ptid = gettid();

// Default values
// char ShortName[20] = "PUB2"; // The size is necessary for the GNU/Linux console codes //~COLOR
char ShortName[20] = CP2"PUB2"CR; // The size is necessary for the GNU/Linux console codes //~COLOR
int TimePeriod = PUB2_PERIOD;

// Component specific parameters
int loop = 1;

// Character set
// Alphabet definition - Lower characters
char AlphabetLower[] = "abcdefghijklmnopqrstuvwxyz";

printf("[%s][%04d] -- %s initialization\n", ShortName, loop, Names[2]);
printf("[%s][INFO] -- Pthread ID: %d\n", ShortName, ptid);
printf("[%s][INFO] -- Time period: %d\n", ShortName, TimePeriod);

// Initial delay
usleep(TimePeriod * 1000); // usleep used for emulating msleep
// usleep(TimePeriod * ptid); // usleep used for emulating msleep

char msg[MessageLength]; // Local copy of message

// Main loop
do {
// Generate random character string and write the generated message to the shared memory
for (int i = 0; i < MessageLength;) {
	int t;
	t = (rand() * getpid()) % sizeof(AlphabetLower);
	if (t < 0) t *= -1; // Make sure the pseudo random number is positive
	msg[i] = AlphabetLower[t];
	if (msg[i] != '\0') i++;
}

printf("[%s][%04d] -- Publishing message: [%s]\n", ShortName, loop, msg);

printf("[%s][%04d] -- Locking mutex\n", ShortName, loop);
pthread_mutex_lock(&mutexMessage); // Lock mutex on Message
printf("[%s][%04d] -- Writing %d bytes to shared memory\n", ShortName, loop, MessageLength);

	memcpy(MessagePublisher2, msg, MessageLength); // Write to shared memory
	NewMessage = 1;

printf("[%s][%04d] -- %d bytes written to shared memory\n", ShortName, loop, sizeof(MessagePublisher2));
// printf("[%s][%04d] -- %d bytes written to shared memory\n", ShortName, loop, MessageLength);
pthread_mutex_unlock(&mutexMessage); // Unlock mutex on Message
printf("[%s][%04d] -- Unlocking mutex\n", ShortName, loop);

// Assume zero delay in producing and transmitting data
// TimePeriod pseudo corresponds to the period of the component
printf("[%s][%04d] -- Suspending publisher 2 for %d ms\n", ShortName, loop, TimePeriod);
usleep(TimePeriod * 1000); // usleep used for emulating msleep

loop++; // Increase loop counter each cycle
} while (ConditionPub2);

// Exit message after receiving signal from signal handler
printf("[%s][%04d] Thread id %d: Exiting %s\n", ShortName, loop, ptid, Names[3]);

	pthread_exit(0);
}

// Subscriber 1
void *subscriber1(void *ptr) {
// Determine pthread identificator
uint64_t ptid = gettid();

// Default values
// char ShortName[20] = "SUB1"; // The size is necessary for the GNU/Linux console codes //~COLOR
char ShortName[20] = CS1"SUB1"CR; // The size is necessary for the GNU/Linux console codes //~COLOR
int TimePeriod = SUB1_PERIOD;

// Component specific parameters
int loop = 1;

printf("[%s][%04d] -- %s initialization\n", ShortName, loop, Names[4]);
printf("[%s][INFO] -- Pthread ID: %d\n", ShortName, ptid);
printf("[%s][INFO] -- Time period: %d\n", ShortName, TimePeriod);
printf("[%s][INFO] -- Message length: %d\n", ShortName, MessageLength);

// Declare local copy of message
char msg[MessageLength]; // Local copy of message
// RequestMessage[MessageLength] = '\0';

// Initial delay
usleep(TimePeriod * 1000); // usleep used for emulating msleep
// usleep(TimePeriod * ptid); // usleep used for emulating msleep

// Main loop
do {
// Write a message request to the mediator via provided pipe FD
printf("[%s][%04d] -- Requesting message...\n", ShortName, loop);
printf("[%s][%04d] -- Locking mutex\n", ShortName, loop);
pthread_mutex_lock(&mutexMessage); // Lock mutex on Message

	NewRequest = 1;
	// NewRequest = NewRequest + 1;
	printf("[%s][%04d] -- New request %d written to shared memory\n", ShortName, loop, NewRequest);

pthread_mutex_unlock(&mutexMessage); // Unlock mutex on Message
printf("[%s][%04d] -- Unlocking mutex\n", ShortName, loop);

printf("[%s][%04d] -- Awaiting message...\n", ShortName, loop);
printf("[%s][%04d] -- Locking mutex for subscriber 1\n", ShortName, loop);
pthread_mutex_lock(&mutexSubscriber1); // Lock mutex on subscriber 1, waiting for mediator

printf("[%s][%04d] -- Reading %d bytes from shared memory\n", ShortName, loop, MessageLength);
memcpy(msg, MessageSubscriber1, MessageLength); // Read from shared memory
if (sizeof(msg) == MessageLength) printf("[%s][%04d] -- Subscribing message: [%s]\n", ShortName, loop, msg);
else printf("[%s][%04d] -- Subscribing message failed\n", ShortName, loop);

// Assume zero delay in producing and transmitting data
// TimePeriod pseudo corresponds to the period of the component
// printf("[%s][%04d] -- Suspending subscriber 1\n", ShortName, loop);
printf("[%s][%04d] -- Suspending subscriber 1 for %d ms\n", ShortName, loop, TimePeriod);
usleep(TimePeriod * 1000); // usleep used for emulating msleep

loop++; // Increase loop counter each cycle
} while (ConditionSub1);

// Exit message after receiving signal from signal handler
printf("[%s][%04d] Thread id %d: Exiting %s\n", ShortName, loop, ptid, Names[4]);

	pthread_exit(0);
}

// Subscriber 2
void *subscriber2(void *ptr) {
// Determine pthread identificator
uint64_t ptid = gettid();

// Default values
// char ShortName[20] = "SUB2"; // The size is necessary for the GNU/Linux console codes //~COLOR
char ShortName[20] = CS2"SUB2"CR; // The size is necessary for the GNU/Linux console codes //~COLOR
int TimePeriod = SUB2_PERIOD;

// Component specific parameters
int loop = 1;

printf("[%s][%04d] -- %s initialization\n", ShortName, loop, Names[5]);
printf("[%s][INFO] -- Pthread ID: %d\n", ShortName, ptid);
printf("[%s][INFO] -- Time period: %d\n", ShortName, TimePeriod);
printf("[%s][INFO] -- Message length: %d\n", ShortName, MessageLength);

// Declare local copy of message
char msg[MessageLength]; // Local copy of message
// RequestMessage[MessageLength] = '\0';

// Initial delay
usleep(TimePeriod * 1000); // usleep used for emulating msleep
// usleep(TimePeriod * ptid); // usleep used for emulating msleep

// Main loop
do {
// Write a message request to the mediator via provided pipe FD
printf("[%s][%04d] -- Requesting message...\n", ShortName, loop);
printf("[%s][%04d] -- Locking mutex\n", ShortName, loop);
pthread_mutex_lock(&mutexMessage); // Lock mutex on Message

	NewRequest = 2;
	// NewRequest = NewRequest + 2;
	printf("[%s][%04d] -- New request %d written to shared memory\n", ShortName, loop, NewRequest);

pthread_mutex_unlock(&mutexMessage); // Unlock mutex on Message
printf("[%s][%04d] -- Unlocking mutex\n", ShortName, loop);

printf("[%s][%04d] -- Awaiting message...\n", ShortName, loop);
printf("[%s][%04d] -- Locking mutex for subscriber 2\n", ShortName, loop);
pthread_mutex_lock(&mutexSubscriber2); // Lock mutex on subscriber 2, waiting for mediator

printf("[%s][%04d] -- Reading %d bytes from shared memory\n", ShortName, loop, MessageLength);
memcpy(msg, MessageSubscriber2, MessageLength); // Read from shared memory
if (sizeof(msg) == MessageLength) printf("[%s][%04d] -- Subscribing message: [%s]\n", ShortName, loop, msg);
else printf("[%s][%04d] -- Subscribing message failed\n", ShortName, loop);

// Assume zero delay in producing and transmitting data
// TimePeriod pseudo corresponds to the period of the component
// printf("[%s][%04d] -- Suspending subscriber 1\n", ShortName, loop);
printf("[%s][%04d] -- Suspending subscriber 2 for %d ms\n", ShortName, loop, TimePeriod);
usleep(TimePeriod * 1000); // usleep used for emulating msleep

loop++; // Increase loop counter each cycle
} while (ConditionSub2);

// Exit message after receiving signal from signal handler
printf("[%s][%04d] Thread id %d: Exiting %s\n", ShortName, loop, ptid, Names[5]);

	pthread_exit(0);
}

// Subscriber 3
void *subscriber3(void *ptr) {
// Determine pthread identificator
uint64_t ptid = gettid();

// Default values
// char ShortName[20] = "SUB3"; // The size is necessary for the GNU/Linux console codes //~COLOR
char ShortName[20] = CS3"SUB3"CR; // The size is necessary for the GNU/Linux console codes //~COLOR
int TimePeriod = SUB3_PERIOD;

// Component specific parameters
int loop = 1;

printf("[%s][%04d] -- %s initialization\n", ShortName, loop, Names[6]);
printf("[%s][INFO] -- Pthread ID: %d\n", ShortName, ptid);
printf("[%s][INFO] -- Time period: %d\n", ShortName, TimePeriod);
printf("[%s][INFO] -- Message length: %d\n", ShortName, MessageLength);

// Declare local copy of message
char msg[MessageLength]; // Local copy of message
// RequestMessage[MessageLength] = '\0';

// Initial delay
usleep(TimePeriod * 1000); // usleep used for emulating msleep
// usleep(TimePeriod * ptid); // usleep used for emulating msleep

// Main loop
do {
// Write a message request to the mediator via provided pipe FD
printf("[%s][%04d] -- Requesting message...\n", ShortName, loop);
printf("[%s][%04d] -- Locking mutex\n", ShortName, loop);
pthread_mutex_lock(&mutexMessage); // Lock mutex on Message

	NewRequest = 3;
	// NewRequest = NewRequest + 4;
	printf("[%s][%04d] -- New request %d written to shared memory\n", ShortName, loop, NewRequest);

pthread_mutex_unlock(&mutexMessage); // Unlock mutex on Message
printf("[%s][%04d] -- Unlocking mutex\n", ShortName, loop);

printf("[%s][%04d] -- Awaiting message...\n", ShortName, loop);
printf("[%s][%04d] -- Locking mutex for subscriber 1\n", ShortName, loop);
pthread_mutex_lock(&mutexSubscriber3); // Lock mutex on subscriber 3, waiting for mediator

printf("[%s][%04d] -- Reading %d bytes from shared memory\n", ShortName, loop, MessageLength);
memcpy(msg, MessageSubscriber3, MessageLength); // Read from shared memory
if (sizeof(msg) == MessageLength) printf("[%s][%04d] -- Subscribing message: [%s]\n", ShortName, loop, msg);
else printf("[%s][%04d] -- Subscribing message failed\n", ShortName, loop);

// Assume zero delay in producing and transmitting data
// TimePeriod pseudo corresponds to the period of the component
// printf("[%s][%04d] -- Suspending subscriber 1\n", ShortName, loop);
printf("[%s][%04d] -- Suspending subscriber 3 for %d ms\n", ShortName, loop, TimePeriod);
usleep(TimePeriod * 1000); // usleep used for emulating msleep

loop++; // Increase loop counter each cycle
} while (ConditionSub1);

// Exit message after receiving signal from signal handler
printf("[%s][%04d] Thread id %d: Exiting %s\n", ShortName, loop, ptid, Names[6]);

	pthread_exit(0);
}
                                                                                                                                                                                                                                                                                                                                  test.cpp                                                                                            0000644 0001750 0001750 00000002522 12656157455 012104  0                                                                                                    ustar   emeres                          emeres                                                                                                                                                                                                                 #include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void *print_message_function( void *ptr );

main()
{
     pthread_t thread1, thread2;
     const char *message1 = "Thread 1";
     const char *message2 = "Thread 2";
     int  iret1, iret2;

    /* Create independent threads each of which will execute function */

     iret1 = pthread_create( &thread1, NULL, print_message_function, (void*) message1);
     if(iret1)
     {
         fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
         exit(EXIT_FAILURE);
     }

     iret2 = pthread_create( &thread2, NULL, print_message_function, (void*) message2);
     if(iret2)
     {
         fprintf(stderr,"Error - pthread_create() return code: %d\n",iret2);
         exit(EXIT_FAILURE);
     }

     printf("pthread_create() for thread 1 returns: %d\n",iret1);
     printf("pthread_create() for thread 2 returns: %d\n",iret2);

     /* Wait till threads are complete before main continues. Unless we  */
     /* wait we run the risk of executing an exit which will terminate   */
     /* the process and all threads before the threads have completed.   */

     pthread_join( thread1, NULL);
     pthread_join( thread2, NULL); 

     exit(EXIT_SUCCESS);
}

void *print_message_function( void *ptr )
{
     char *message;
     message = (char *) ptr;
     printf("%s \n", message);
}
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              