/*
Assignment 02 - Advanced and Robot Programming
Publisher, subscriber and mediator scheme in POSIX environment using pthread
Ernest Skrzypczyk -- 4268738 -- EMARO // ES4268738-AaRP-AS02
*/
// Pthread
// Compile with:
//	g++ -Wall -lpthread -o AS02B ES4268738-AaRP-AS02B.cpp
// #include "ES4268738-AaRP-AS02.h"
#include "ES4268738-AaRP-AS02b.h"
#include <fcntl.h> //semaphore
#include <inttypes.h> //uint64
#include <iostream>
#include <iomanip>
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
#include <queue>

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
pthread_mutex_t mutexRequest = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condSubscriber1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t condSubscriber2 = PTHREAD_COND_INITIALIZER;
pthread_cond_t condSubscriber3 = PTHREAD_COND_INITIALIZER; 
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
	pthread_mutex_unlock(&mutexRequest);
	pthread_cond_signal(&condSubscriber1);
	pthread_cond_signal(&condSubscriber2);
	pthread_cond_signal(&condSubscriber3);
	pthread_mutex_unlock(&mutexSubscriber1);
	pthread_mutex_unlock(&mutexSubscriber2);
	pthread_mutex_unlock(&mutexSubscriber3);
	///TODO
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
// char MessageSubscriber1[MSG_LENGTH];
// char MessageSubscriber2[MSG_LENGTH];
// char MessageSubscriber3[MSG_LENGTH];
std::string MessageSubscriber1;
std::string MessageSubscriber2;
std::string MessageSubscriber3;
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

// Convert char array to string
std::string sconvert(const char *pCh, int arraySize)
{
	std::string str;
	if (pCh[arraySize-1] == '\0') str.append(pCh);
	else for(int i = 0; i < arraySize; i++) str.append(1, pCh[i]);
	return str;
}

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

printf("[%s][%04d] -- %s initialization\n", ShortName, loop, Names[1]);
printf("[%s][INFO] -- Pthread ID: %d\n", ShortName, ptid);
printf("[%s][INFO] -- Time period: %d\n", ShortName, TimePeriod);

// Clear mediator circular buffer
std::vector<std::string> CircularBuffer;
std::vector<int> MetadataBuffer;
CircularBuffer.reserve(MessageBuffer);
MetadataBuffer.reserve(MessageBuffer);

int MessageCounter = 0;
int mt = 0;
int rmc = 0;

// Initial delay
usleep(TimePeriod * 1000); // usleep used for emulating msleep
// usleep(TimePeriod * ptid); // usleep used for emulating msleep

char msg[MessageLength]; // Local copy of message
std::string smsg; // Local copy of message as string

// Main loop
do {

pthread_mutex_lock(&mutexMessage); // Lock mutex on Message
// if (NewMessage) { // Process a message per cycle, risk of loosing a message
while (NewMessage) { // Process all new messages in one cycle
printf("[%s][%04d] -- Locking mutex\n", ShortName, loop);

	// /* Check if the circular buffer is full
	if (MessageCounter >= MessageBuffer) {
		printf("[%s][%04d] -- Circular buffer full, current capacity %d\n", ShortName, loop, CircularBuffer.capacity());
		CircularBuffer.pop_back(); // Delete last element
		MetadataBuffer.pop_back(); // Delete last element

		printf("[%s][%04d] -- Circular and metadata buffer content\n", ShortName, loop);
		printf("[%s][%04d] -- #  [ MSG  ][ META ]\n", ShortName, loop);
		// for (int i = 0; i < MessageBuffer; i++) {
		for (int i = 0; i < CircularBuffer.size(); i++) {
			printf("[%s][%04d] -- #%d [ ", ShortName, loop, i + 1);
			std::cout << CircularBuffer[i] << " ][ " << MetadataBuffer[i]; // Needed for properly printing strings
			printf(" ]\n");
		}
		MessageCounter = CircularBuffer.size();
	}
	// */

	printf("[%s][%04d] -- Publisher messages %d before processing\n", ShortName, loop, NewMessage);

	if (NewMessage / 2) { // Start from the end
		printf("[%s][%04d] -- Reading %d bytes from shared memory from publisher 2\n", ShortName, loop, MessageLength);

		memcpy(msg, MessagePublisher2, MessageLength); // Read from shared memory
		memset(MessagePublisher2, 0, MessageLength); // Delete message from shared memory
		NewMessage = NewMessage - 2; // Clear new message indicator for publisher 2

		printf("[%s][%04d] -- %d bytes read from shared memory\n", ShortName, loop, sizeof(msg));
	} else if (NewMessage / 1) {
		printf("[%s][%04d] -- Reading %d bytes from shared memory from publisher 1\n", ShortName, loop, MessageLength);

		memcpy(msg, MessagePublisher1, MessageLength); // Read from shared memory
		memset(MessagePublisher1, 0, MessageLength); // Delete message from shared memory
		NewMessage = NewMessage - 1; // Clear new message indicator for publisher 1

		printf("[%s][%04d] -- %d bytes read from shared memory\n", ShortName, loop, sizeof(msg));
	}

	printf("[%s][%04d] -- Reading message from publisher: [ ", ShortName, loop);
	std::cout << sconvert(msg, MessageLength);
	printf(" ]\n");

printf("[%s][%04d] -- Unlocking mutex\n", ShortName, loop);
pthread_mutex_unlock(&mutexMessage); // Unlock mutex on Message

	// Send message to buffer
	if (*(msg) != '\0') {
		// Processing message
		// Conversion to string
		smsg = sconvert(msg, MessageLength);
		printf("[%s][%04d] -- Sending message [ ", ShortName, loop);
		std::cout << smsg; // Needed for properly printing strings
		printf(" ] to buffer\n");

		/* Alternative conversion to string
		// smsg = '\0';
		// for (int i = 0; msg[i] != 0; i++) smsg += msg[i]; // Contruct a message with metadata
		// smsg += '\0';
		// */

		// Always send new messages to the first element pushing the queue till end
		CircularBuffer.insert(CircularBuffer.begin(), smsg);
		MetadataBuffer.insert(MetadataBuffer.begin(), 7); // 2^0 + 2^1 + 2^2 = 1 + 2 + 4 = 7

		/* Alternative approaches
		// CircularBuffer.push_back(smsg);
		// MetadataBuffer.push_back(7); // 2^0 + 2^1 + 2^2 = 1 + 2 + 4 = 7

		// CircularBuffer.insert(CircularBuffer.begin() + MessageCounter, smsg);
		// MetadataBuffer.insert(MetadataBuffer.begin() + MessageCounter, 7); // 2^0 + 2^1 + 2^2 = 1 + 2 + 4 = 7

		// MetadataBuffer.push_back(MessageCounter); // 2^0 + 2^1 + 2^2 = 1 + 2 + 4 = 7
		// printf("[%s][%04d] -- Message [%s] stored in circular buffer\n", ShortName, loop, CircularBuffer(CircularBuffer.end()));
		// */

		// for (int i = 0; i < MessageBuffer; i++) {
		// for (int i = 0; i < CircularBuffer.size(); i++) {
		MessageCounter = CircularBuffer.size();
		for (int i = 0; i < MessageCounter; i++) {
			printf("[%s][%04d] -- #%d [ ", ShortName, loop, i + 1);
			std::cout << CircularBuffer[i] << " ][ " << MetadataBuffer[i]; // Needed for properly printing strings
			printf(" ]\n");

		// if (MessageCounter < MessageBuffer) MessageCounter++;
		// if (MessageCounter < MessageBuffer) MessageCounter = CircularBuffer.size();
		}
		// for (unsigned int i = 0; i < CircularBuffer.size(); i++) printf("[%s][%04d] -- [ %s ][ %d ]\n", ShortName, loop, CircularBuffer[i], MetadataBuffer[i]);
	}
	printf("[%s][%04d] -- Publisher messages %d after processing\n", ShortName, loop, NewMessage);
	// memset(msg, 0, MessageLength); // Delete message from shared memory
	printf("[%s][%04d] -- Unlocking mutex\n", ShortName, loop);
}
pthread_mutex_unlock(&mutexMessage); // Unlock mutex on Message



pthread_mutex_lock(&mutexRequest); // Lock mutex on Request
// if (NewRequest) { // Each operation per one cycle
while (NewRequest) { // Process all requests in one cycle
printf("[%s][%04d] -- Locking mutex for requests\n", ShortName, loop);
	printf("[%s][%04d] -- Subscriber requests %d before processing\n", ShortName, loop, NewRequest);
	rmc = 0;

	/* Lock all request threads
	pthread_mutex_lock(&mutexSubscriber1); // Lock mutex on Request
	pthread_mutex_lock(&mutexSubscriber2); // Lock mutex on Request
	pthread_mutex_lock(&mutexSubscriber3); // Lock mutex on Request
	// */

	if (NewRequest / 4) {
		// Subscriber 3
		printf("[%s][%04d] -- Serving subscriber 3\n", ShortName, loop);

		// Get message from buffer
		MessageCounter = CircularBuffer.size();
		for (unsigned int i = MessageCounter - 1; i >= 0; i--) {
			// Load message from queue
			smsg = CircularBuffer.at(i);
			mt = MetadataBuffer.at(i);

			printf("[%s][%04d] -- Processing message and metadata #%d [ ", ShortName, loop, i + 1);
			std::cout << smsg;
			printf(" ][ %d ]\n", mt);

			if (mt / 4) {
				rmc = i;
				break;
			}
		}

		// Sending message to subscriber
		pthread_mutex_lock(&mutexSubscriber3); // Lock mutex on Request
		printf("[%s][%04d] -- Writing %d bytes to shared memory\n", ShortName, loop, MessageLength);
		// printf("[%s][%04d] -- Writing %d bytes to shared memory\n", ShortName, loop, sizeof(CircularBuffer.at(rmc)));

		MessageSubscriber3 = CircularBuffer.at(rmc); // Write to shared memory

		// printf("[%s][%04d] -- %d bytes written to shared memory\n", ShortName, loop, sizeof(smsg));
		printf("[%s][%04d] -- %d bytes written to shared memory\n", ShortName, loop, MessageLength);
		RequestGranted = 3;
		printf("[%s][%04d] -- Granting request %d\n", ShortName, loop, RequestGranted);
		// Update request indicator
		NewRequest = NewRequest - 4;

		// Update tags on message
		MetadataBuffer[rmc] = mt - 4;
		printf("[%s][%04d] -- Processed message and metadata [ ", ShortName, loop);
		std::cout << smsg;
		printf(" ][ %d ]\n", MetadataBuffer.at(rmc));

		pthread_cond_signal(&condSubscriber3); // Unlock condition variable
		pthread_mutex_unlock(&mutexSubscriber3); // Unlock mutex
		printf("[%s][%04d] -- Unlocking mutex for subscriber 3\n", ShortName, loop);

	} else if (NewRequest / 2) {
		// Subscriber 2
		printf("[%s][%04d] -- Serving subscriber 2\n", ShortName, loop);

		// Get message from buffer
		MessageCounter = CircularBuffer.size();
		for (unsigned int i = MessageCounter - 1; i >= 0; i--) {
			// Load message from queue
			smsg = CircularBuffer.at(i);
			mt = MetadataBuffer.at(i);

			printf("[%s][%04d] -- Processing message and metadata #%d [ ", ShortName, loop, i + 1);
			std::cout << smsg;
			printf(" ][ %d ]\n", mt);

			if (mt / 2) {
				rmc = i;
				break;
			}
		}

		// Sending message to subscriber
		pthread_mutex_lock(&mutexSubscriber2); // Lock mutex on Request
		printf("[%s][%04d] -- Writing %d bytes to shared memory\n", ShortName, loop, MessageLength);

		MessageSubscriber2 = CircularBuffer.at(rmc); // Write to shared memory

		printf("[%s][%04d] -- %d bytes written to shared memory\n", ShortName, loop, MessageLength);
		RequestGranted = 2;
		printf("[%s][%04d] -- Granting request %d\n", ShortName, loop, RequestGranted);
		// Update request indicator
		NewRequest = NewRequest - 2;

		// Update tags on message
		MetadataBuffer[rmc] = mt - 2;
		printf("[%s][%04d] -- Processed message and metadata [ ", ShortName, loop);
		std::cout << smsg;
		printf(" ][ %d ]\n", MetadataBuffer.at(rmc));

		pthread_cond_signal(&condSubscriber2); // Unlock condition variable
		pthread_mutex_unlock(&mutexSubscriber2); // Unlock mutex
		printf("[%s][%04d] -- Unlocking mutex for subscriber 2\n", ShortName, loop);

	} else if (NewRequest / 1) {
		// Subscriber 1
		printf("[%s][%04d] -- Serving subscriber 1\n", ShortName, loop);

		// Get message from buffer
		MessageCounter = CircularBuffer.size();
		for (unsigned int i = MessageCounter - 1; i >= 0; i--) {
			// Load message from queue
			smsg = CircularBuffer.at(i);
			mt = MetadataBuffer.at(i);

			printf("[%s][%04d] -- Processing message and metadata #%d [ ", ShortName, loop, i + 1);
			std::cout << smsg;
			printf(" ][ %d ]\n", mt);

			if (mt / 1) {
				rmc = i;
				break;
			}
		}

		// Sending message to subscriber
		pthread_mutex_lock(&mutexSubscriber1); // Lock mutex on Request
		printf("[%s][%04d] -- Writing %d bytes to shared memory\n", ShortName, loop, MessageLength);
		// printf("[%s][%04d] -- Writing %d bytes to shared memory\n", ShortName, loop, sizeof(CircularBuffer.at(rmc)));

		// MessageSubscriber1 = smsg; // Write to shared memory
		MessageSubscriber1 = CircularBuffer.at(rmc); // Write to shared memory

		// printf("[%s][%04d] -- %d bytes written to shared memory\n", ShortName, loop, sizeof(smsg));
		printf("[%s][%04d] -- %d bytes written to shared memory\n", ShortName, loop, MessageLength);
		RequestGranted = 1;
		printf("[%s][%04d] -- Granting request %d\n", ShortName, loop, RequestGranted);
		// Update request indicator
		NewRequest = NewRequest - 1;

		// Update tags on message
		MetadataBuffer[rmc] = mt - 1;
		printf("[%s][%04d] -- Processed message and metadata [ ", ShortName, loop);
		std::cout << smsg;
		printf(" ][ %d ]\n", MetadataBuffer.at(rmc));

		pthread_cond_signal(&condSubscriber1); // Unlock condition variable
		pthread_mutex_unlock(&mutexSubscriber1); // Unlock mutex
		printf("[%s][%04d] -- Unlocking mutex for subscriber 1\n", ShortName, loop);

	}

	// /* Check if any message has fullfiled metadata and delete accordingly
	// Alternatively use the above processing routines and set a flag to delete a message
	// This approach is safer, but iterates through all objects
	printf("[%s][%04d] -- Check and delete unnecessary messages from buffers\n", ShortName, loop);
	MessageCounter = CircularBuffer.size();
	for (unsigned int i = MessageCounter - 1; i > 0; i--) {
		// Load message from queue
		// printf("Checking message: msg counter %d\n", MessageCounter);//DB
		// printf("Checking message: interation %d\n", i);//DB
		smsg = CircularBuffer.at(i);
		mt = MetadataBuffer.at(i);

		printf("[%s][%04d] -- Checking message and metadata #%d [ ", ShortName, loop, i + 1);
		std::cout << smsg;
		printf(" ][ %d ]\n", MetadataBuffer.at(i));

		// if (mt == 0) {
		if (!mt) {
			printf("[%s][%04d] -- Deleting message and metadata #%d from buffers\n", ShortName, loop, i + 1);
			CircularBuffer.erase(CircularBuffer.begin() + i);
			MetadataBuffer.erase(MetadataBuffer.begin() + i);
			// Alternative
			// CircularBuffer.erase(CircularBuffer.end() - i);
			// MetadataBuffer.erase(MetadataBuffer.end() - i);
			MessageCounter = CircularBuffer.size(); // Update message counter

			printf("[%s][%04d] -- Circular and metadata buffer content\n", ShortName, loop);
			printf("[%s][%04d] -- #  [ MSG  ][ META ]\n", ShortName, loop);
			// for (int i = 0; i < MessageBuffer; i++) {
			for (int i = 0; i < MessageCounter; i++) {
				printf("[%s][%04d] -- #%d [ ", ShortName, loop, i + 1);
				std::cout << CircularBuffer[i] << " ][ " << MetadataBuffer[i]; // Needed for properly printing strings
				printf(" ]\n");
		}

			continue;
		}
	}
	MessageCounter = CircularBuffer.size();
	// */

	printf("[%s][%04d] -- Subscriber requests %d (composite, not decimal) after processing\n", ShortName, loop, NewRequest);

	// /* Debug display subscriber message queue
	printf("[%s][%04d] -- Subscriber output queue:\n", ShortName, loop);
	printf("[%s][%04d] -- [ SUB1 ][ SUB2 ][ SUB3 ]\n", ShortName, loop);
	printf("[%s][%04d] -- [ ", ShortName, loop);
	std::cout << MessageSubscriber1;
	printf(" ][ ");
	std::cout << MessageSubscriber2;
	printf(" ][ ");
	std::cout << MessageSubscriber3;
	printf(" ]\n");
	// */

	printf("[%s][%04d] -- Unlocking mutex for requests\n", ShortName, loop);
}
pthread_mutex_unlock(&mutexRequest); // Unlock mutex on Request

/* Unlock all mutexes
pthread_mutex_unlock(&mutexSubscriber2); // Unlock mutex on Message
pthread_mutex_unlock(&mutexSubscriber3); // Unlock mutex on Message
// */

// printf("[%s][%04d] -- Suspending mediator\n", ShortName, loop);//VB
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
// usleep(TimePeriod * 1000); // usleep used for emulating msleep
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
	// NewMessage = 1;
	NewMessage += 1;

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
	// NewMessage = 2;
	NewMessage += 2;

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
std::string smsg; // Local copy of string message

// Initial delay
usleep(TimePeriod * 1000); // usleep used for emulating msleep
// usleep(TimePeriod * ptid); // usleep used for emulating msleep

// Main loop
do {
// Write a message request to the mediator via provided pipe FD
printf("[%s][%04d] -- Requesting message...\n", ShortName, loop);
printf("[%s][%04d] -- Locking mutex\n", ShortName, loop);
pthread_mutex_lock(&mutexRequest); // Lock mutex on subscriber 1, waiting for mediator
// pthread_mutex_lock(&mutexMessage); // Lock mutex on Message

	NewRequest = NewRequest + 1;
	printf("[%s][%04d] -- New request %d written to shared memory\n", ShortName, loop, NewRequest);

// pthread_mutex_unlock(&mutexMessage); // Unlock mutex on Message
pthread_mutex_unlock(&mutexRequest); // Unlock mutex on subscriber 1, waiting for mediator
printf("[%s][%04d] -- Unlocking mutex\n", ShortName, loop);

printf("[%s][%04d] -- Awaiting message...\n", ShortName, loop);
printf("[%s][%04d] -- Locking mutex for subscriber 1\n", ShortName, loop);
pthread_mutex_lock(&mutexSubscriber1); // Lock mutex on subscriber 1, waiting for mediator
pthread_cond_wait(&condSubscriber1, &mutexSubscriber1); // Wait for condition from mutex on subscriber 1, waiting for mediator
// pthread_cond_wait(&condSubscriber1, &mutexRequest); // Wait for condition from mutex on subscriber 1, waiting for mediator
// printf("[%s][%04d] -- Lock released\n", ShortName, loop);

// pthread_mutex_lock(&mutexSubscriber1); // Lock mutex on Message

	// printf("[%s][%04d] -- Lock released\n", ShortName, loop);
	printf("[%s][%04d] -- Reading %d bytes from shared memory\n", ShortName, loop, MessageLength);
	smsg = MessageSubscriber1; // Read from shared memory
	MessageSubscriber1 = '\0'; // Clear message from subscribers queue
	// The size does not correspond correctly to content
	printf("[%s][%04d] -- Subscribing message: [ ", ShortName, loop);
	std::cout << smsg;
	printf(" ]\n", ShortName, loop);

pthread_mutex_unlock(&mutexSubscriber1); // Unlock mutex on Message

///FIX
// printf("[%s][%04d] -- Subscribing message parameters: %d\n", ShortName, loop, sizeof(smsg));
/* The size does not correspond correctly to content
if (sizeof(smsg) == MessageLength) {
	// printf("[%s][%04d] -- Subscribing message: [%s]\n", ShortName, loop, smsg);
	printf("[%s][%04d] -- Subscribing message: [ ", ShortName, loop);
	std::cout << smsg;
	printf(" ]\n", ShortName, loop);
} else printf("[%s][%04d] -- Subscribing message failed\n", ShortName, loop);
// */

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
std::string smsg;
// RequestMessage[MessageLength] = '\0';

// Initial delay
usleep(TimePeriod * 1000); // usleep used for emulating msleep
// usleep(TimePeriod * ptid); // usleep used for emulating msleep

// Main loop
do {
// Write a message request to the mediator via provided pipe FD
printf("[%s][%04d] -- Requesting message...\n", ShortName, loop);
printf("[%s][%04d] -- Locking mutex\n", ShortName, loop);
pthread_mutex_lock(&mutexRequest); // Lock mutex on subscriber 2, waiting for mediator

	NewRequest = NewRequest + 2;
	printf("[%s][%04d] -- New request %d written to shared memory\n", ShortName, loop, NewRequest);

pthread_mutex_unlock(&mutexRequest); // Unlock mutex on subscriber 2, waiting for mediator
printf("[%s][%04d] -- Unlocking mutex\n", ShortName, loop);

printf("[%s][%04d] -- Awaiting message...\n", ShortName, loop);
printf("[%s][%04d] -- Locking mutex for subscriber 2\n", ShortName, loop);
pthread_mutex_lock(&mutexSubscriber2); // Lock mutex on subscriber 2, waiting for mediator
pthread_cond_wait(&condSubscriber2, &mutexSubscriber2); // Wait for condition from mutex on subscriber 1, waiting for mediator
// pthread_cond_wait(&condSubscriber2, &mutexRequest); // Wait for condition from mutex on subscriber 1, waiting for mediator
// printf("[%s][%04d] -- Lock released\n", ShortName, loop);

// pthread_mutex_lock(&mutexSubscriber2); // Lock mutex on Message

	// printf("[%s][%04d] -- Lock released\n", ShortName, loop);
	printf("[%s][%04d] -- Reading %d bytes from shared memory\n", ShortName, loop, MessageLength);
	smsg = MessageSubscriber2; // Read from shared memory
	MessageSubscriber2 = '\0';
	// The size does not correspond correctly to content
	printf("[%s][%04d] -- Subscribing message: [ ", ShortName, loop);
	std::cout << smsg;
	printf(" ]\n", ShortName, loop);

pthread_mutex_unlock(&mutexSubscriber2); // Unlock mutex on Message

///FIX
// printf("[%s][%04d] -- Subscribing message parameters: %d\n", ShortName, loop, sizeof(smsg));
/* The size does not correspond correctly to content
if (sizeof(smsg) == MessageLength) {
	// printf("[%s][%04d] -- Subscribing message: [%s]\n", ShortName, loop, smsg);
	printf("[%s][%04d] -- Subscribing message: [ ", ShortName, loop);
	std::cout << smsg;
	printf(" ]\n", ShortName, loop);
} else printf("[%s][%04d] -- Subscribing message failed\n", ShortName, loop);
// */

// Assume zero delay in producing and transmitting data
// TimePeriod pseudo corresponds to the period of the component
// printf("[%s][%04d] -- Suspending subscriber 2\n", ShortName, loop);
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
std::string smsg;
// RequestMessage[MessageLength] = '\0';

// Initial delay
usleep(TimePeriod * 1000); // usleep used for emulating msleep
// usleep(TimePeriod * ptid); // usleep used for emulating msleep

// Main loop
do {
// Write a message request to the mediator via provided pipe FD
printf("[%s][%04d] -- Requesting message...\n", ShortName, loop);
printf("[%s][%04d] -- Locking mutex\n", ShortName, loop);
pthread_mutex_lock(&mutexRequest); // Lock mutex on subscriber 3, waiting for mediator

	NewRequest = NewRequest + 4;
	printf("[%s][%04d] -- New request %d written to shared memory\n", ShortName, loop, NewRequest);

pthread_mutex_unlock(&mutexRequest); // Unlock mutex on subscriber 3, waiting for mediator
printf("[%s][%04d] -- Unlocking mutex\n", ShortName, loop);

printf("[%s][%04d] -- Awaiting message...\n", ShortName, loop);
printf("[%s][%04d] -- Locking mutex for subscriber 3\n", ShortName, loop);
pthread_mutex_lock(&mutexSubscriber3); // Lock mutex on subscriber 3, waiting for mediator
pthread_cond_wait(&condSubscriber3, &mutexSubscriber3); // Wait for condition from mutex on subscriber 1, waiting for mediator
// pthread_cond_wait(&condSubscriber3, &mutexRequest); // Wait for condition from mutex on subscriber 1, waiting for mediator

// pthread_mutex_lock(&mutexSubscriber3); // Lock mutex on Message

	// printf("[%s][%04d] -- Lock released\n", ShortName, loop);
	printf("[%s][%04d] -- Reading %d bytes from shared memory\n", ShortName, loop, MessageLength);
	smsg = MessageSubscriber3; // Read from shared memory
	MessageSubscriber3 = '\0';
	// The size does not correspond correctly to content
	printf("[%s][%04d] -- Subscribing message: [ ", ShortName, loop);
	std::cout << smsg;
	printf(" ]\n", ShortName, loop);

pthread_mutex_unlock(&mutexSubscriber3); // Unlock mutex on Message

///FIX
// printf("[%s][%04d] -- Subscribing message parameters: %d\n", ShortName, loop, sizeof(smsg));
/* The size does not correspond correctly to content
if (sizeof(smsg) == MessageLength) {
	// printf("[%s][%04d] -- Subscribing message: [%s]\n", ShortName, loop, smsg);
	printf("[%s][%04d] -- Subscribing message: [ ", ShortName, loop);
	std::cout << smsg;
	printf(" ]\n", ShortName, loop);
} else printf("[%s][%04d] -- Subscribing message failed\n", ShortName, loop);
// */

// Assume zero delay in producing and transmitting data
// TimePeriod pseudo corresponds to the period of the component
// printf("[%s][%04d] -- Suspending subscriber 3\n", ShortName, loop);
printf("[%s][%04d] -- Suspending subscriber 3 for %d ms\n", ShortName, loop, TimePeriod);
usleep(TimePeriod * 1000); // usleep used for emulating msleep

loop++; // Increase loop counter each cycle
} while (ConditionSub1);

// Exit message after receiving signal from signal handler
printf("[%s][%04d] Thread id %d: Exiting %s\n", ShortName, loop, ptid, Names[6]);

	pthread_exit(0);
}
