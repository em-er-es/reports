#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> //fork
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h> //wait

main()
{
pid_t Pid0, Pid1; // PID variables
int FileDescriptor[2], Status;
Pid0 = getpid(); // getpid never fails
printf("Father PID: %d.\nInput Array elements:\n", Pid0);
int ne = 4; // Number of elements of the array to be transmitted
int InputArray[ne]; // Declaration of the array
for (int i = 0; i < ne; i++) {
	scanf("%d", &InputArray[i]); // Reading from user input
}
pipe(FileDescriptor); //Open pipe for processes
Pid1 = fork(); // Creating a child process
if (Pid1 < 0) { // Error handling
	fprintf(stderr, "Error forking process: %d\n", Pid1); exit(1);
	}
else if (Pid1 == 0) { // Child
	int ReadArray[ne];
	close(FileDescriptor[1]); // Close writing end of pipe
	printf("Child PID: %d\n", Pid1 = getpid());
	printf("Receiving data from father process %d\n", Pid0);
	int nb = read(FileDescriptor[0], &ReadArray, sizeof ReadArray); //Read pipe
	int i = 0;
	printf("%d bytes received through the pipe\n", nb);
	
	printf("\nReceived array before sorting:\n");
	for (int i = 0; i < ne; i++) printf("%d ", ReadArray[i]);
	// Sort array
	//~ for (int i = 0; i < ne; i++) {
		//~ for (int j = i; j < ne; j++) {
			//~ if (ReadArray[i] > ReadArray[j]) {
				//~ ReadArray[i] = ReadArray[j];
			//~ }
		//~ }
	//~ }
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
	printf("\nReceived array after sorting:\n");
	for (int i = 0; i < ne; i++) printf("%d ", ReadArray[i]);
	printf("\n");
} else { // Father
	close(FileDescriptor[0]); // Close reading end of pipe
	printf("Sending data to child process %d\n", Pid1);
	int bw = write(FileDescriptor[1], &InputArray, sizeof InputArray);
	close(FileDescriptor[1]); // Close writing end of pipe, child process sees EOF
	printf("%d bytes written to the pipe\nData send to child process %d\n", bw, Pid1);
}
exit(0);
}
