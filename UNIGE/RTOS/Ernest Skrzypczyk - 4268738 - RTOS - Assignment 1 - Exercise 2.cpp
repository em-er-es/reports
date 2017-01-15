/*
Ernest Skrzypczyk - 4268738 - EMARO+ 1 year
Assignment 1 - Exercise 2
Earliest Deadline First
*/

/*
 Please execute the code by running:
 g++ 'Ernest Skrzypczyk - 4268738 - RTOS - Assignment 1 - Exercise 2.cpp' -lpthread -o rtos-as01-ex02
 schedtool -a 0x01 -e ./rtos-as01-ex02
*/
 
/* 
 There is segmentation fault if the code is executed in its current state.
 If lines 354 -- 357 are commented out, it will run. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>

//Declaration of functions:
//Application code of each task
//Thread functions 
//3 tasks with periods 100 ms, 200 ms, 400 ms

//EDF scheduler - check and assign priorities
void edf();
void printinfo();

// Declaration of two variables for maximum and minimum priority
struct sched_param priomax;
struct sched_param priomin;

//Periodic tasks
void task1_code();
void *task1(void *);
void task2_code();
void *task2(void *);
void task3_code();
void *task3(void *);

//Declaration of arrays that can be used for:
// task periods
// arrival time in the following period
// computational time of each task
// scheduling attributes
// thread identifiers
// scheduling parameters
// number of missed deadline

#define NTASKS 3
#define NPERIODICTASKS 3
#define NAPERIODICTASKS 0
#define NLOOPS 100

int missed_deadlines[NTASKS];
int timetowait[NTASKS];
long int periods[NTASKS];
long int priorities[NTASKS];
long int WCET[NTASKS];
pthread_attr_t attributes[NTASKS];
pthread_t thread_id[NTASKS];
struct sched_param parameters[NTASKS];
struct timeval next_arrival_time[NTASKS];

main()
{
  // Storage of two variables for maximum and minimum priority
  priomax.sched_priority = sched_get_priority_max(SCHED_FIFO);
  priomin.sched_priority = sched_get_priority_min(SCHED_FIFO);
  
  // Set task periods in nanoseconds
  periods[0] = 100000000; // 100 000 000 ns = 100 000 µs = 100 ms
  periods[1] = 200000000; // 200 000 000 ns = 200 000 µs = 200 ms
  periods[2] = 400000000; // 400 000 000 ns = 400 000 µs = 400 ms
  //~ priorities[0] = priomin.sched_priority + NTASKS - 1;
  //~ priorities[1] = 2;
  //~ priorities[2] = 1;
  for (int i = 0; i < NTASKS; i++)
	priorities[i] = priomin.sched_priority + NTASKS - (i + 1);
/*
	// Random priorities assignments for first cycle
	priorities[i] = ceil(rand()); 
*/

  // If root, set the maximum priority to the current thread
  if (getuid() == 0)
     pthread_setschedparam(pthread_self(), SCHED_FIFO, &priomax);

  // Measure execution times for tasks in standalone modality using gettimeofday function
  // Update the worst case execution time
  for (int i = 0; i < NTASKS; i++)
    {
      struct timeval timeval1;
      struct timezone timezone1;
      struct timeval timeval2;
      struct timezone timezone2;
      gettimeofday(&timeval1, &timezone1);

      if (i==0)
       task1_code();
      if (i==1)
       task2_code();
      if (i==2)
       task3_code();
       
      gettimeofday(&timeval2, &timezone2);

      WCET[i]= 1000 * ((timeval2.tv_sec - timeval1.tv_sec) * 1000000 + (timeval2.tv_usec - timeval1.tv_usec));
      printf("\nWorst Case Execution Time %d = %d\n", i, WCET[i]);
    }

  //Calculate U_lub and U
  double Ulub = 1;
  double U = 0;

  for (int i = 0; i < NTASKS; i++)
    U+= ((double)WCET[i]) / ((double)periods[i]);

  // Verify schedulability
  if (U > Ulub)
    {
      printf("\nU = %lf Ulub = %lf -- Non schedulable Task Set", U, Ulub);
      return(-1);
    }
  printf("\nU = %lf Ulub = %lf -- Scheduable Task Set\n", U, Ulub);
  fflush(stdout);

  // If root, set the minimum priority to the current thread
  if (getuid() == 0)
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &priomin);

  // Setting the attributes of each task, including scheduling policy and priority
  for (int i = 0; i < NTASKS; i++)
    {
      pthread_attr_init(&(attributes[i]));

      pthread_attr_setinheritsched(&(attributes[i]), PTHREAD_EXPLICIT_SCHED);
      pthread_attr_setschedpolicy(&(attributes[i]), SCHED_FIFO);
 
      parameters[i].sched_priority = priomin.sched_priority + NTASKS - (i + 1);
      pthread_attr_setschedparam(&(attributes[i]), &(parameters[i]));
    }
  printinfo();
  printf("\nExecution schedule legend:\nP[n] - periodic task [n];\n\n");
  printf("\nExecution schedule: \n");
  sleep(3);
  
  int iret[NTASKS];
  struct timeval now;
  struct timezone zona;
  gettimeofday(&now, &zona);
 
  // Read the current time (gettimeofday) and set the next arrival time for each task 
  // (i.e., the beginning of the next period)

  for (int i = 0; i < NTASKS; i++)
    {
      long int periods_micro = periods[i]/1000;
      next_arrival_time[i].tv_sec = now.tv_sec + periods_micro/1000000;
      next_arrival_time[i].tv_usec = now.tv_usec + periods_micro%1000000;
      missed_deadlines[i] = 0;
    }

  // Create all threads
  iret[0] = pthread_create( &(thread_id[0]), &(attributes[0]), task1, NULL);
  iret[1] = pthread_create( &(thread_id[1]), &(attributes[1]), task2, NULL);
  iret[2] = pthread_create( &(thread_id[2]), &(attributes[2]), task3, NULL);

  // Join all threads
  pthread_join( thread_id[0], NULL);
  pthread_join( thread_id[1], NULL); 
  pthread_join( thread_id[2], NULL);
  
  printf("\n");
  // Return value 0 for successful execution
  exit(0);
}

// Application specific tasks and threads code
void task1_code()
{
  int i, j; 
  double uno;
  for (i = 0; i < 10; i++)
    {
      for (j = 0; j < 10000; j++)
	{
		uno = rand() * rand();
    	}
  }
  printf("P1;");
  fflush(stdout);
}

void *task1(void *ptr)
{
  struct timespec waittime;
  waittime.tv_sec = 0; /* seconds */
  waittime.tv_nsec = periods[0]; /* nanoseconds */

  for (int i = 0; i < NLOOPS; i++)
    {
      // execute application specific code
      task1_code();
      // after execution, compute the time to the beginning of the next period
      struct timeval now;
      struct timezone zona;
      gettimeofday(&now, &zona);

      timetowait[0] = 1000*((next_arrival_time[0].tv_sec - now.tv_sec)*1000000
                                +(next_arrival_time[0].tv_usec - now.tv_usec));
      if (timetowait[0] < 0)
		missed_deadlines[0]++;

      waittime.tv_sec = timetowait[0] / 1000000000;
      waittime.tv_nsec = timetowait[0] % 1000000000;
      
      // suspend the task until the beginning of the next period
      // (use nanosleep)
      nanosleep(&waittime, NULL);

      // the task is ready: set the next arrival time for each task
      // (i.e., the beginning of the next period)
      long int periods_micro=periods[0]/1000;
      next_arrival_time[0].tv_sec = next_arrival_time[0].tv_sec + periods_micro/1000000;
      next_arrival_time[0].tv_usec = next_arrival_time[0].tv_usec + periods_micro%1000000;
    
    edf();
    }
}

void task2_code()
{
  double due;
  for (int i = 0; i < 20; i++)
    {
      for (int j = 0; j < 10000; j++)
	{
	due = rand()*rand();
	}
    }
  printf("P2;");
  fflush(stdout);
}

void *task2(void *ptr )
{
  struct timespec waittime;
  waittime.tv_sec=0; /* seconds */
  waittime.tv_nsec = periods[1]; /* nanoseconds */
  for (int i = 0; i < NLOOPS; i++)
    {
      task2_code();
      struct timeval now;
      struct timezone zona;
      gettimeofday(&now, &zona);
      timetowait[1] = 1000*((next_arrival_time[1].tv_sec -
				  now.tv_sec)*1000000 +(next_arrival_time[1].tv_usec-now.tv_usec));
      if (timetowait[1] < 0)
      missed_deadlines[1]++;
      waittime.tv_sec = timetowait[1] / 1000000000;
      waittime.tv_nsec = timetowait[1] % 1000000000;
      nanosleep(&waittime, NULL);
      long int periods_micro = periods[1]/1000;
      next_arrival_time[1].tv_sec = next_arrival_time[1].tv_sec + periods_micro/1000000;
      next_arrival_time[1].tv_usec = next_arrival_time[1].tv_usec + periods_micro%1000000;

	edf();
    }
}

void task3_code()
{
  double tre;
  for (int i = 0; i < 10; i++)
    {
      for (int j = 0; j < 10000; j++)
	      tre = rand()*rand();
    }
  printf("P3;");
  fflush(stdout);
}

void *task3(void *ptr )
{
  struct timespec waittime;
  waittime.tv_sec=0; /* seconds */
  waittime.tv_nsec = periods[2]; /* nanoseconds */
  for (int i = 0; i < NLOOPS; i++)
    {
      task2_code();
      struct timeval now;
      struct timezone zona;
      gettimeofday(&now, &zona);
      timetowait[2] = 1000*((next_arrival_time[1].tv_sec -
				  now.tv_sec)*1000000 +(next_arrival_time[1].tv_usec-now.tv_usec));
      if (timetowait[2] < 0)
      missed_deadlines[2]++;
      waittime.tv_sec = timetowait[2] / 1000000000;
      waittime.tv_nsec = timetowait[2] % 1000000000;
      nanosleep(&waittime, NULL);
      long int periods_micro=periods[1]/1000;
      next_arrival_time[1].tv_sec = next_arrival_time[1].tv_sec + periods_micro/1000000;
      next_arrival_time[1].tv_usec = next_arrival_time[1].tv_usec + periods_micro%1000000;

	edf();
    }
}

int compare_function(const void *a, const void *b)
{
	int *x = (int *) a;
	int *y = (int *) b;
	return *x - *y;
}

void printinfo()
{
  printf("\nPriorities, periods, WCETs, time2wait:\n");
  for (int j = 0; j < NTASKS; j++)
    printf("Task %d: %d %d %d %f %d\n", j, priorities[j], parameters[j].sched_priority, periods[j], double(WCET[j]), timetowait[j]);
}

void edf()
{
	int minindex; int mintime = 2147483647; // Maximum value for int type
	int maxindex; int maxtime = 0;
	int midindex; int midtime = 0;
	for (int i = 0; i < NTASKS; i++)
		for (int j = 0; j < NTASKS; j++) {
			if (timetowait[i] < timetowait[j])
			{
			  mintime = timetowait[i]; minindex = i;
			}
			if (timetowait[i] > timetowait[j])
			{
			  maxtime = timetowait[i]; maxindex = i;
			}
			if (timetowait[i] < maxtime && timetowait[i] > mintime)
			{
			  midtime = timetowait[i]; midindex = i;
			}
		}
      // This should be not too difficult to expand to a higher number of processes
      parameters[minindex].sched_priority = priomin.sched_priority;
      pthread_attr_setschedparam(&(attributes[minindex]), &(parameters[minindex]));
      // Comment this out for a proper run -- START
      parameters[midindex].sched_priority = priomin.sched_priority + 1;
      pthread_attr_setschedparam(&(attributes[midindex]), &(parameters[midindex]));
      // Comment this out for a proper run -- END
      //~ parameters[maxindex].sched_priority = priomin.sched_priority + NTASKS - 1;
      parameters[maxindex].sched_priority = priomin.sched_priority + 2;
      pthread_attr_setschedparam(&(attributes[maxindex]), &(parameters[maxindex]));
	// Comment out the last line for conventional schedule information
	printinfo();
}
