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
     
     //printf("Base pointer (0x%08x), Current pointer (0x%08x), New pointer (0x%08x)\n", base_ptr, curr_ptr, new_ptr);
     //printf("Base pointer (%d), Current pointer (%d), New pointer (%d)\n", *base_ptr, *curr_ptr, *new_ptr);
     cs1550_down(full);
     sleep(5);
     printf("Semaphore value (%d)\n", full->value);
     //printf("Base pointer (%d), Current pointer (%d), New pointer (%d)\n", *base_ptr, *curr_ptr, *new_ptr);
     cs1550_up(full);
     printf("Semaphore value (%d)\n", full->value);
     //printf("Base pointer (%d), Current pointer (%d), New pointer (%d)\n", *base_ptr, *curr_ptr, *new_ptr);
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
