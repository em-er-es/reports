/*
Ernest Skrzypczyk - 4268738 - EMARO+ 1 year
Assignment 1 - Exercise 1
Rate monotonic scheduling for periodic tasks, polling server for aperiodic tasks
*/

/*
 Please execute the code by running:
 g++ 'Ernest Skrzypczyk - 4268738 - RTOS - Assignment 1 - Exercise 1.cpp' -lpthread -o rtos-as01-ex01
 schedtool -a 0x01 -e ./rtos-as01-ex01
*/
 
/* 
 The mutexes for aperiodic tasks were left in place in this version,
 so the conditions for mutexes are also used. As you can see, I tried
 using binary conditions for the aperiodic task scheduling variable
 *aperiodicschedule*, apparently my programming skills in C/C++ are too
 rusty to make it work properly. Only aperiodic task 1 / task 4 is
 executed when binary conditions are used, so there is a programming 
 error there, which is the reason I switched to convetional variables.
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
//1 polling server
//2 aperiodic tasks

//Periodic tasks
void task1_code( );
void *task1(void *);
void task2_code( );
void *task2(void *);
void task3_code( );
void *task3(void *);

//Polling server
void taskps_code();
void *taskps(void *);

//Aperiodic tasks
void task4_code( );
void *task4(void *);
void task5_code( );
void *task5(void *);

//Declaration of arrays that can be used for:
// task periods
// arrival time in the following period
// computational time of each task
// scheduling attributes
// thread identifiers
// scheduling parameters
// number of missed deadline

pthread_mutex_t mutex_task_4 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_task_5 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_task_4 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_task_5 = PTHREAD_COND_INITIALIZER;

#define NTASKS 6
#define NPERIODICTASKS 4
#define NAPERIODICTASKS 2
#define NLOOPS 100

int missed_deadlines[NTASKS];
long int periods[NTASKS];
long int WCET[NTASKS];
pthread_attr_t attributes[NTASKS];
pthread_t thread_id[NTASKS];
struct sched_param parameters[NTASKS];
struct timeval next_arrival_time[NTASKS];

// Binary conditions for aperiodic tasks
//~ bool aperiodicschedule = 00; 
bool aperiodicschedule1 = 0; 
bool aperiodicschedule2 = 0; 

main()
{
  // Set task periods in nanoseconds
  // It is interesting to see how the schedule looks with other values:
  // periods[0]= 50000000; // Polling server: 50 000 000 ns = 50 000 µs = 50 ms
  periods[0] = 10000000;
  // periods[0] = 220000000;
  // periods[0] = 380000000;
  // periods[0] = 730000000;
  periods[1]= 100000000; // 100 000 000 ns = 100 000 µs = 100 ms
  periods[2]= 200000000; // 200 000 000 ns = 200 000 µs = 200 ms
  periods[3]= 400000000; // 400 000 000 ns = 400 000 µs = 400 ms
  periods[4]= 0; // Aperiodic task 1
  periods[5]= 0; // Aperiodic task 2

  // Declaration and storage of two variables for maximum and minimum priority
  struct sched_param priomax;
  priomax.sched_priority = sched_get_priority_max(SCHED_FIFO);
  struct sched_param priomin;
  priomin.sched_priority = sched_get_priority_min(SCHED_FIFO);

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
       taskps_code();
      if (i==1)
       task1_code();
      if (i==2)
       task2_code();
      if (i==3)
       task3_code();
      if (i==4)
       task4_code();
      if (i==5)
       task5_code();
       
      gettimeofday(&timeval2, &timezone2);

      WCET[i]= 1000 * ((timeval2.tv_sec - timeval1.tv_sec) * 1000000 + (timeval2.tv_usec - timeval1.tv_usec));
      printf("\nWorst Case Execution Time %d = %d\n", i, WCET[i]);
    }

  //Calculate U_lub and U
  double Ulub = NPERIODICTASKS * (pow(2.0, (1.0 / NPERIODICTASKS)) - 1);
  double U = 0;

  for (int i = 1; i < NPERIODICTASKS; i++)
    U+= ((double)WCET[i]) / ((double)periods[i]);
  printf("\nU_Period_tasks = %f", U);

  double WCETA_SUM; // or long int and then convert to double?
  for (int i = NPERIODICTASKS; i < NTASKS; i++) // Aperiodic tasks only
    WCETA_SUM+= (double)WCET[i]; // Calculating computation time for period tasks

  // Calculating the relation of computing time for aperiodic tasks in 
  // relation to the period of the polling server, which is then added to U
  U+= ((double)WCETA_SUM) / ((double)periods[0]); 
  //~ printf("WCET_A = %f\n", WCETA_SUM);
  
  //Verify schedulability
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
  
      // Polling server
      pthread_attr_init(&(attributes[0]));

      pthread_attr_setinheritsched(&(attributes[0]), PTHREAD_EXPLICIT_SCHED);
      pthread_attr_setschedpolicy(&(attributes[0]), SCHED_FIFO);
 
      //Highest ("lowest") priority for polling server
      parameters[0].sched_priority = priomax.sched_priority; 
      pthread_attr_setschedparam(&(attributes[0]), &(parameters[0]));

  for (int i = 1; i < NPERIODICTASKS; i++) // Omitting polling server
    {
      pthread_attr_init(&(attributes[i]));

      pthread_attr_setinheritsched(&(attributes[i]), PTHREAD_EXPLICIT_SCHED);
      pthread_attr_setschedpolicy(&(attributes[i]), SCHED_FIFO);
 
      parameters[i].sched_priority = priomin.sched_priority + NTASKS - i;
      pthread_attr_setschedparam(&(attributes[i]), &(parameters[i]));
    }
    
  for (int i = NPERIODICTASKS; i < NTASKS; i++)
    {
      pthread_attr_init(&(attributes[i]));
      pthread_attr_setschedpolicy(&(attributes[i]), SCHED_FIFO);

      parameters[i].sched_priority = priomin.sched_priority + NTASKS - i;
      pthread_attr_setschedparam(&(attributes[i]), &(parameters[i]));
    }

  printf("\nPriorities, periods and WCET:\n");
  printf("Task 0: %d %d %f -- Polling server\n", parameters[0].sched_priority, periods[0], double(WCET[0]));
  for (int i = 1; i < NTASKS; i++)
    printf("Task %d: %d %d %f \n", i, parameters[i].sched_priority, periods[i], double(WCET[i]));
  printf("\nExecution schedule legend:\nP[n] - periodic task [n];\nPS - polling server;\nC[n] - condition for executing aperiodic task met;\nE[n] - execution call for aperiodic task;\nA[n]-T[m] - actual execution of aperiodic task [n] as task [m]\n");
  printf("\nExecution schedule: \n");
  sleep(3);
  
  int iret[NTASKS];
  struct timeval ora;
  struct timezone zona;
  gettimeofday(&ora, &zona);
 
  // Read the current time (gettimeofday) and set the next arrival time for each task 
  // (i.e., the beginning of the next period)

  for (int i = 0; i < NPERIODICTASKS; i++)
    {
      long int periods_micro = periods[i]/1000;
      next_arrival_time[i].tv_sec = ora.tv_sec + periods_micro/1000000;
      next_arrival_time[i].tv_usec = ora.tv_usec + periods_micro%1000000;
      missed_deadlines[i] = 0;
    }

  // Create all threads
  iret[0] = pthread_create( &(thread_id[0]), &(attributes[0]), taskps, NULL);
  iret[1] = pthread_create( &(thread_id[1]), &(attributes[1]), task1, NULL);
  iret[2] = pthread_create( &(thread_id[2]), &(attributes[2]), task2, NULL);
  iret[3] = pthread_create( &(thread_id[3]), &(attributes[3]), task3, NULL);
  iret[4] = pthread_create( &(thread_id[4]), &(attributes[4]), task4, NULL);
  iret[5] = pthread_create( &(thread_id[5]), &(attributes[5]), task5, NULL);

  // Join all threads
  pthread_join( thread_id[0], NULL);
  pthread_join( thread_id[1], NULL);
  pthread_join( thread_id[2], NULL);
  pthread_join( thread_id[3], NULL);
  
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
  // When the random variable uno < 0.025, 
  // then aperiodic task 4 must be scheduled to be executed
  if (uno < 0.025)
    {
      //~ printf(":C4;"); aperiodicschedule = aperiodicschedule | 01; fflush(stdout);
      printf(":C4;"); aperiodicschedule1 = 1; fflush(stdout);
    }

  // When the random variable uno > 0.995,
  // then aperiodic task 5 must be scheduled to be executed
  if (uno > 0.995)
    {
      //~ printf(":C5;"); aperiodicschedule = aperiodicschedule | 10; fflush(stdout);
      printf(":C5;"); aperiodicschedule2 = 1; fflush(stdout);
    }
  fflush(stdout);
}

//thread code for task_1 (used only for temporization)

void *task1(void *ptr)
{
  struct timespec waittime;
  waittime.tv_sec=0; /* seconds */
  waittime.tv_nsec = periods[1]; /* nanoseconds */

  for (int i = 0; i < NLOOPS; i++)
    {
      // execute application specific code
      task1_code();
      // after execution, compute the time to the beginning of the next period
      struct timeval ora;
      struct timezone zona;
      gettimeofday(&ora, &zona);

      long int timetowait= 1000*((next_arrival_time[1].tv_sec - ora.tv_sec)*1000000
                                +(next_arrival_time[1].tv_usec-ora.tv_usec));
      if (timetowait <0)
      	missed_deadlines[1]++;

      waittime.tv_sec = timetowait/1000000000;
      waittime.tv_nsec = timetowait%1000000000;
      
        // suspend the task until the beginning of the next period
	// (use nanosleep)
     
	// the task is ready: set the next arrival time for each task
	// (i.e., the beginning of the next period)
      nanosleep(&waittime, NULL);

      // the task is ready: set the next arrival time for each task
      // (i.e., the beginning of the next period)

      long int periods_micro=periods[1]/1000;
      next_arrival_time[1].tv_sec = next_arrival_time[1].tv_sec +
       periods_micro/1000000;
      next_arrival_time[1].tv_usec = next_arrival_time[1].tv_usec +
       periods_micro%1000000;
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
  waittime.tv_nsec = periods[2]; /* nanoseconds */
  for (int i = 0; i < NLOOPS; i++)
    {
      task2_code();
      struct timeval ora;
      struct timezone zona;
      gettimeofday(&ora, &zona);
      long int timetowait= 1000*((next_arrival_time[2].tv_sec -
				  ora.tv_sec)*1000000 +(next_arrival_time[2].tv_usec-ora.tv_usec));
      if (timetowait <0)
	missed_deadlines[2]++;
      waittime.tv_sec = timetowait/1000000000;
      waittime.tv_nsec = timetowait%1000000000;
      nanosleep(&waittime, NULL);
      long int periods_micro=periods[1]/1000;
      next_arrival_time[2].tv_sec = next_arrival_time[2].tv_sec + periods_micro/1000000;
      next_arrival_time[2].tv_usec = next_arrival_time[2].tv_usec +
	periods_micro%1000000;
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

void *task3(void *ptr)
{
  struct timespec waittime;
  waittime.tv_sec = 0; /* seconds */
  waittime.tv_nsec = periods[3]; /* nanoseconds */
  for (int i = 0; i < NLOOPS; i++)
    {
      task3_code();
      struct timeval ora;
      struct timezone zona;
      gettimeofday(&ora, &zona);
      long int timetowait= 1000*((next_arrival_time[3].tv_sec - ora.tv_sec)*1000000
				 +(next_arrival_time[3].tv_usec-ora.tv_usec));
      if (timetowait < 0)
	missed_deadlines[3]++;
      waittime.tv_sec = timetowait/1000000000;
      waittime.tv_nsec = timetowait%1000000000;
      nanosleep(&waittime, NULL);
      long int periods_micro=periods[3]/1000;
      next_arrival_time[3].tv_sec = next_arrival_time[3].tv_sec +
	periods_micro/1000000;
      next_arrival_time[3].tv_usec = next_arrival_time[3].tv_usec +
	periods_micro%1000000;
    }
}

void task4_code()
{
  double quattro;
  for (int i = 0; i < 10; i++)
    {
      for (int j = 0; j < 1000; j++)
        quattro = rand() * rand();
    }
  printf("A1-T4;");
  fflush(stdout);
}

void *task4(void *)
{
  while (1)
    {
      // Waiting for the polling server to signal the condition
      pthread_mutex_lock(&mutex_task_4);
      pthread_cond_wait(&cond_task_4, &mutex_task_4);
      pthread_mutex_unlock(&mutex_task_4);
      task4_code();
    }
}

void task5_code()
{
  for (int i = 0; i < 10; i++)
    {
      for (int j = 0; j < 1000; j++)
        double cinque = rand() * rand();
    }
  printf("A2-T5;");
  fflush(stdout);
}

void *task5(void *)
{
  while (1)
    {
      // Waiting for the polling server to signal the condition
      pthread_mutex_lock(&mutex_task_5);
      pthread_cond_wait(&cond_task_5, &mutex_task_5);
      pthread_mutex_unlock(&mutex_task_5);
      task5_code();
    }
}

void taskps_code()
{
  double tmp;
  for (int i = 0; i < NLOOPS; i++)
    {
      for (int j = 0; j < 100; j++)
	{
	tmp = rand() * rand();
	}
    }
  printf("PS;");
  // When the random variable uno < 0.025, then aperiodic task 4 must
  // be executed
  //~ if (aperiodicschedule & 01)
  if (aperiodicschedule1 == 1)
    {
      printf(":E4;"); fflush(stdout);
      pthread_mutex_lock(&mutex_task_4);
      pthread_cond_signal(&cond_task_4);
      pthread_mutex_unlock(&mutex_task_4);
      // Clearing the execution condition
      //~ aperiodicschedule = aperiodicschedule & 10;
      aperiodicschedule1 = 0;
    }

  // When the random variable uno > 0.995, then aperiodic task 5 must
  // be executed
  //~ if (aperiodicschedule & 10)
  if (aperiodicschedule2 == 1)
    {
      printf(":E5;"); fflush(stdout);
      pthread_mutex_lock(&mutex_task_5);
      pthread_cond_signal(&cond_task_5);
      pthread_mutex_unlock(&mutex_task_5);
      // Clearing the execution condition
      //~ aperiodicschedule = aperiodicschedule & 01;
      aperiodicschedule2 = 0;
    }
  fflush(stdout);
}

void *taskps(void *ptr)
{
  int i=0;
  struct timespec waittime;
  waittime.tv_sec=0; /* seconds */
  waittime.tv_nsec = periods[0]; /* nanoseconds */
  for (i=0; i < NLOOPS; i++)
    {
      taskps_code();
      struct timeval ora;
      struct timezone zona;
      gettimeofday(&ora, &zona);
      long int timetowait= 1000*((next_arrival_time[0].tv_sec -
				  ora.tv_sec)*1000000 +(next_arrival_time[0].tv_usec-ora.tv_usec));
      if (timetowait <0)
	missed_deadlines[0]++;
      waittime.tv_sec = timetowait/1000000000;
      waittime.tv_nsec = timetowait%1000000000;
      nanosleep(&waittime, NULL);
      long int periods_micro=periods[0]/1000;
      next_arrival_time[0].tv_sec = next_arrival_time[0].tv_sec + periods_micro/1000000;
      next_arrival_time[0].tv_usec = next_arrival_time[0].tv_usec +
	periods_micro%1000000;
    }
}
