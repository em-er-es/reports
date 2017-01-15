#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> //fork
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h> //wait

main()
{
pid_t Pid0, Pid1; // PID variables
pid_t Wait1; // State of child process

Pid0 = getpid(); // getpid() never fails
printf("Father PID: %d.\n", Pid0);
Pid1 = fork(); // Creating a child process

if (Pid0 == getpid()) printf("Child PID from father: %d\n", Pid1);
if (Pid1 < 0) { // Error handling
	fprintf(stderr, "Error forking process: %d\n", Pid1);
	exit(1);
	}
else if (Pid1 == 0) { // Child
	printf("Child PID: %d\n", Pid1 = getpid());
	printf("Executing child process\n");
	sleep(4);
	//~ kill(Pid0, 15); // man 7 signal -- 15 Termination signal
	//~ kill(Pid0, 20); // man 7 signal -- 20 Termination of child signal
	kill(Pid0, SIGCHLD); // man 7 signal -- 20 Termination of child signal
	//~ kill(Pid0, SIGKILL); // man 7 signal -- 9 Termination
	printf("Child process %d sends signal SIGCHLD to its Father process %d\n", Pid1, Pid0);
} else { // Father
	sleep(1);
	printf("Father process %d awaits state change (signal SIGCHLD) from its child process %d\n", Pid0, Pid1);
	waitpid(Pid1, &Wait1, 0); // Waiting for state change of child process
	printf("Father process received state change through signal SIGCHLD from child process %d\n", Pid1);
	sleep(1);
	printf("Father process %d still running\n", Pid0);
	sleep(1);
	printf("Father process %d exiting\n", Pid0);
}
exit(0);
}
