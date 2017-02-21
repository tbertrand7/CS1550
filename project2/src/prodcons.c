/* 
 * Tom Bertrand
 * 2/20/2017
 * COE 1550
 * Misurda T, Th 11:00
 * Project 2
 * Program to implement prodcons solution
 * 
 * Built using skeleton code from:
 * (C) Mohammad H. Mofrad, 2017  
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <linux/prodcons.h>

void *BASE_PTR;
void *BUFFER_PTR;
void *SHARED_VARS_PTR;
int *CURR_PTR;

void main(int argc, char *argv[])
{
     if (argc != 4)
     {
          printf("Usage: ./prodcons [num_producers] [num_consumers] [buffer_size] \n");
          exit(1);
     }

     /* Comand line args */
     int num_producers = atoi(argv[1]);
     int num_consumers = atoi(argv[2]);
     int buffer_size = atoi(argv[3]);

     BASE_PTR = (void *) mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
     if(BASE_PTR == (void *) -1) 
     {
          fprintf(stderr, "Error mapping memory\n");
          exit(1);
     }     
     CURR_PTR = BASE_PTR; //Initialize CURR_PTR to start of map

     /* Initialize semaphores */
     struct cs1550_sem *empty = cs1550_sem_init(0);
     struct cs1550_sem *full = cs1550_sem_init(buffer_size);
     struct cs1550_sem *mutex = cs1550_sem_init(1);

     /* Allocate memory for the shared buffer */
     BUFFER_PTR = (void *) mmap(NULL, sizeof(int)*buffer_size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
     if(BUFFER_PTR == (void *) -1) 
     {
          fprintf(stderr, "Error mapping buffer memory\n");
          exit(1);
     }

     /* Allocate memory for the shared variables */
     SHARED_VARS_PTR = (void *) mmap(NULL, sizeof(int)*3, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

     /* Shared pointers for producers and consumers */
     int *buffer_ptr = (int *)BUFFER_PTR;
     int *buffer_size_ptr = (int *)SHARED_VARS_PTR + 1;
     int *producer_ptr = (int *)SHARED_VARS_PTR + 2;
     int *consumer_ptr = (int *)SHARED_VARS_PTR + 3;

     /* Initialize shared variable values*/
     *buffer_size_ptr = buffer_size;
     *producer_ptr = 0;
     *consumer_ptr = 0;
     
     int i;
     //Initialize buffer to 0s
     for (i=0; i < buffer_size; i++)
     {
          buffer_ptr[i] = 0;
     }

     //Create producer threads
     for (i=0; i < num_producers; i++)
     {
          if (fork() == 0)
          {
               int item;

               while(1)
               {
                    cs1550_down(empty);
                    cs1550_down(mutex); //Enter mutex

                    item = *producer_ptr; //Get next sequential int for buffer
                    buffer_ptr[*producer_ptr % *buffer_size_ptr] = item; //Use mod to produce within buffer bounds
                    printf("Producer %c Produced: %d\n", i+65, item);
                    *producer_ptr = *producer_ptr++; //Increment producer_ptr

                    cs1550_up(mutex); //Release mutex
                    cs1550_up(full);
               }
          }
     }


     //Create consumer threads
     for (i=0; i < num_consumers; i++)
     {
          if (fork() == 0)
          {
               int item;

               while(1)
               {
                    cs1550_down(full);
                    cs1550_down(mutex); //Enter mutex

                    item = buffer_ptr[*consumer_ptr]; //Retrieve next int from buffer
                    printf("Consumer %c Consumed: %d\n", i+65, item);
                    *consumer_ptr = (*consumer_ptr++) % *buffer_size_ptr; //Increment consumer_ptr to next buffer location

                    cs1550_up(mutex); //Release mutex
                    cs1550_up(empty);
               }
          }
     }
}

/* Initialize cs1550_sem */
struct cs1550_sem* cs1550_sem_init(int val)
{
     //Check address for semaphore
     CURR_PTR = check_sem_address(CURR_PTR);
     struct cs1550_sem *sem = (struct cs1550_sem *) CURR_PTR;

     CURR_PTR = CURR_PTR + sizeof(struct cs1550_sem); //Increase pointer for next semaphore

     //Set initial semaphore values
     sem->value = val;
     sem->start = NULL;
     sem->end = NULL;

     return sem;
}

/* Check pointer for the semaphore is within the map range */
int* check_sem_address(int *curr_ptr)
{
     int *base_ptr = BASE_PTR;
     int *new_ptr;
     int size = sizeof(struct cs1550_sem);
     curr_ptr = curr_ptr + size;
     if(curr_ptr > base_ptr + MAP_SIZE) 
     {
          fprintf(stderr, "Address out of range\n");
          exit(1);
     }
     else
     {
          new_ptr = curr_ptr - size;
     }

     return new_ptr;
}

void cs1550_down(struct cs1550_sem *sem) 
{
     syscall(__NR_sys_cs1550_down, sem);
}

void cs1550_up(struct cs1550_sem *sem) 
{
     syscall(__NR_sys_cs1550_up, sem);
}
