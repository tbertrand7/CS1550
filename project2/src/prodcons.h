/* 
 * Tom Bertrand
 * 2/20/2017
 * COE 1550
 * Misurda T, Th 11:00
 * Project 2
 * Header for prodcons implementation
 * 
 * Built using skeleton code from:
 * (C) Mohammad H. Mofrad, 2017  
 */

#ifndef PRODCONS_H_INCLUDED
#define PRODCONS_H_INCLUDED


#include <asm/errno.h>
#include <asm/unistd.h>
#include <asm/mman.h>

#define MAP_SIZE (sizeof(struct cs1550_sem) * 3) //Map enough memory for the three semaphores

/* Node struct for linked list queue */
struct Node
{
	struct task_struct *task;
	struct Node *next;
};

/* Semaphore struct */
struct cs1550_sem
{
   int value;
   struct Node *start; //Start address of sleeping process queue
   struct Node *end; //End address of sleeping process queue
};

struct cs1550_sem* cs1550_sem_init(int val);
int* check_sem_address(int *curr_ptr);

void  cs1550_down(struct cs1550_sem *);
void  cs1550_up  (struct cs1550_sem *);

#endif
