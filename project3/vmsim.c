/* 
 * Tom Bertrand
 * 3/28/2017
 * COE 1550
 * Misurda T, Th 11:00
 * Project 3
 * Virtual Memory with a single level 32-Bit page table and
 * opt, nru, random, and clock page replacement algorithms
 *
 * Built using skeleton code from:
 * (c) Mohammad H. Mofrad, 2017
 * (e) hasanzadeh@cs.pitt.edu
 */

#include "vmsim.h"

// Comment below to see more logs
#undef ALL
#undef DEBUG
#undef INFO

int numframes, refresh;
unsigned int *physical_frames;

// Page Table
unsigned int *page_table = NULL;
// Page Table Entry
struct pte_32 *pte = NULL;

// Fifo page replacement current index
int current_index = -1;

//Pointer for clock
struct frame_struct *clock_pointer;

//Vars for opt
int *pages_to_evict; //Holds frame #'s of pages that will never be used again
int p2e_count = 0; //Count of how many pages are in pages_to_evict stack
int *counts; //Holds times for when each frame will be used in future

int main(int argc, char *argv[])
{
   /*
    * Add sanity check for input arguments
    * Open the memory trace file 
    * and store it in an array
    */

   FILE *file;
   if((argc == 6) && (!strcmp(argv[1],"-n")) && (!strcmp(argv[3], "-a")) \
      && (!strcmp(argv[4], "fifo") || !strcmp(argv[4], "opt") || !strcmp(argv[4], "clock") || !strcmp(argv[4], "rand")) \
      && ((!strcmp(argv[5], "gcc.trace")) || (!strcmp(argv[5], "bzip.trace"))))
   {
      numframes = atoi(argv[2]);
      refresh = 0;
      file = fopen(argv[5],"rb");
      if(!file)
      {
         fprintf(stderr, "USAGE: %s -n <numframes> -a <opt|clock|nru|rand> [-r <refresh>] <tracefile>\n", argv[0]);
         fprintf(stderr, "Error on opening the trace file\n");
         exit(1);
      }
   }
   //Special case for nru for refresh arg
   else if ((argc == 8) && (!strcmp(argv[1],"-n")) && (!strcmp(argv[3], "-a")) && (!strcmp(argv[5], "-r")) \
      && (!strcmp(argv[4], "nru")) && ((!strcmp(argv[7], "gcc.trace")) || (!strcmp(argv[7], "bzip.trace"))))
   {
      numframes = atoi(argv[2]);
      refresh = atoi(argv[6]);
      file = fopen(argv[7],"rb");
      if(!file)
      {
         fprintf(stderr, "USAGE: %s -n <numframes> -a <opt|clock|nru|rand> [-r <refresh>] <tracefile>\n", argv[0]);
         fprintf(stderr, "Error on opening the trace file\n");
         exit(1);
      }
   }
   else
   {
      fprintf(stderr, "USAGE: %s -n <numframes> -a <opt|clock|nru|rand> [-r <refresh>] <tracefile>\n", argv[0]);
      exit(1);
   }

   //Allocate arrays for opt
   pages_to_evict = malloc(sizeof(int)*numframes);
   counts = malloc(sizeof(int)*numframes);
   memset(counts, -1, sizeof(int)*numframes);

   /* 
    * Calculate the trace file's length
    * and read in the trace file
    * and write it into addr_arr and mode_arr arrays 
    */

   unsigned int numaccesses = 0;
   unsigned int addr = 0;
   unsigned char mode = NULL;

   // Calculate number of lines in the trace file
   while(fscanf(file, "%x %c", &addr, &mode) == 2)
   {
      numaccesses++;
   }
   rewind(file);

   unsigned int address_array[numaccesses];
   unsigned char mode_array[numaccesses];
   unsigned int i = 0;

   // Store the memory accesses 
   while(fscanf(file, "%x %c", &address_array[i], &mode_array[i]) == 2)
   {
      #ifdef ALL
         printf("\'0x%08x %c\'\n", address_array[i], mode_array[i]);
      #endif
      i++;
   }

   if(!fclose(file))
   {
      ; // Noop
   }
   else
   {
      fprintf(stderr, "Error on closing the trace file\n");
      exit(1);
   }

   // Initialize the physical memory address space
   physical_frames = malloc(PAGE_SIZE_4KB * numframes);
   if(!physical_frames)
   {
      fprintf(stderr, "Error on mallocing physical frames\n");
      exit(1);
   }
   memset(physical_frames, 0, PAGE_SIZE_4KB * numframes);

   // Create the first frame of the frames linked list
   struct frame_struct *frame = malloc(sizeof(struct frame_struct));
   if(!frame)
   {
      fprintf(stderr, "Error on mallocing frame struct\n");
      exit(1);
   }
   memset(frame, 0, sizeof(struct frame_struct));

   // Store the head of frames linked list
   struct frame_struct *head = frame;

   clock_pointer = frame; //Set clock pointer to head of list

   struct frame_struct *curr;


   // Initialize page table
   page_table = malloc(PT_SIZE_1MB * PTE_SIZE_BYTES);
   if(!page_table)
   {
      fprintf(stderr, "Error on mallocing page table\n");
   }
   memset(page_table, 0, PT_SIZE_1MB * PTE_SIZE_BYTES);

   struct pte_32 *new_pte = NULL;

   // Initialize the frames linked list
   for(i = 0; i < numframes; i++)
   {
      frame->frame_number = i;
      frame->physical_address = physical_frames + (i * PAGE_SIZE_4KB) / PAGE_SIZE_BYTES;
      frame->virtual_address = 0;
      frame->pte_pointer = NULL;
      #ifdef INFO
         printf("Frame#%d: Adding a new frame at memory address %ld(0x%08x)\n", i, frame->physical_address, frame->physical_address);
      #endif
      frame->next = malloc(sizeof(struct frame_struct));
      frame = frame->next;
      memset(frame, 0, sizeof(struct frame_struct));
   }

   unsigned int fault_address = 0;
   unsigned int previous_fault_address = 0;
   unsigned char mode_type = NULL;
   int hit = 0;
   int page2evict = 0;
   int numfaults = 0;
   int numwrites = 0;

   srand(time(NULL)); //Seed rand

   // Main loop to process memory accesses
   for(i = 0; i < numaccesses; i++)
   {
      fault_address = address_array[i];
      mode_type = mode_array[i];
      hit = 0;

      //Refresh pages based on refresh rate for nru
      if (!strcmp(argv[4], "nru") && !(i % refresh))
      {
         curr = head;
         while(curr->next)
         {
            if (curr->pte_pointer)
            {
               curr->pte_pointer->referenced = 0;
            }
            curr = curr->next;
         }
      }

      // Perform the page walk for the fault address
      new_pte = (struct pte_32 *) handle_page_fault(fault_address);

      /*
       * Traverse the frames linked list    
       * to see if the requested page is present in
       * the frames linked list.
       */

      curr = head;
      while(curr->next)
      {
         if(curr->physical_address == new_pte->physical_address)
         {
            if(new_pte->present)
            {
               curr->virtual_address = fault_address;
               curr->pte_pointer->referenced = 1;
               if(mode_type == 'W')
               {
                  curr->pte_pointer->dirty = 1;
               }
               hit = 1;
            }
            break;
         }
         else
         {
            curr = curr->next;
         }
      }

      /* 
       * if the requested page is not present in the
       * frames linked list use the fifo page replacement
       * to evict the victim frame and
       * swap in the requested frame
       */  

      if(!hit)
      {
         //Determine page replacement algorithm
         if(!strcmp(argv[4], "fifo"))
         {
            page2evict = fifo();
         }
         else if(!strcmp(argv[4], "opt"))
         {
            page2evict = opt_alg(head, i, address_array, numaccesses);
         }
         else if(!strcmp(argv[4], "clock"))
         {
            page2evict = clock_alg(head);
         }
         else if(!strcmp(argv[4], "nru"))
         {
            page2evict = nru_alg(head);
         }
         else if (!strcmp(argv[4], "rand"))
         {
            page2evict = rand_alg(head);
         }

         /* Traverse the frames linked list to
          * find the victim frame and swap it out
          * Set the present, referenced, and dirty bits
          * and collect some statistics
          */

         curr = head;
         while(curr->next)
         {
            if(curr->frame_number == page2evict)
            {
               previous_fault_address = curr->virtual_address;
               numfaults++;

               if(curr->pte_pointer)
               {
                  curr->pte_pointer->present = 0;

                  if (curr->pte_pointer->referenced)
                  {
                     curr->pte_pointer->referenced = 0;
                  }

                  if(curr->pte_pointer->dirty)
                  {
                     curr->pte_pointer->dirty = 0;
                     numwrites++; 
                     #ifdef DEBUG
                        printf("%5d: page fault – evict dirty(0x%08x)accessed(0x%08x)\n", i, previous_fault_address, fault_address);
                     #endif 
                  }
                  else
                  {
                     #ifdef DEBUG
                        printf("%5d: page fault – evict clean(0x%08x)accessed(0x%08x)\n", i, previous_fault_address, fault_address);
                     #endif
                  }
               }

               curr->pte_pointer = (struct pte_32 *) new_pte;
               new_pte->physical_address = curr->physical_address;
               new_pte->present = 1;
               new_pte->referenced = 1;
               curr->virtual_address = fault_address;

               if(mode_type == 'W')
               {
                  new_pte->dirty = 1;
               }
            }
            curr = curr->next; 
         }
      }
      else
      {
         #ifdef DEBUG
            printf("%5d: page hit   – no eviction(0x%08x)\n", i, fault_address);
            printf("%5d: page hit   - keep page  (0x%08x)accessed(0x%08x)\n", i, new_pte->physical_address, curr->virtual_address);
         #endif
      }
   }

   free(pages_to_evict);
   free(counts);

   printf("Algorithm:             %s\n", argv[4]);
   printf("Number of frames:      %d\n", numframes);
   printf("Total memory accesses: %d\n", i);
   printf("Total page faults:     %d\n", numfaults);
   printf("Total writes to disk:  %d\n", numwrites);

   return(0);
}

struct frame_struct * handle_page_fault(unsigned int fault_address)
{
   pte = (struct pte_32 *) page_table[PTE32_INDEX(fault_address)];

   if(!pte)
   {
      pte = malloc(sizeof(struct pte_32));
      memset(pte, 0, sizeof(struct pte_32));
      pte->present = 0;
      pte->physical_address = NULL;
      page_table[PTE32_INDEX(fault_address)] = (unsigned int) pte;
   }

   #ifdef INFO
      printf("Page fault handler\n");
      printf("Fault address %d(0x%08x)\n", (unsigned int) fault_address, fault_address);
      printf("Page table base address %ld(0x%08x)\n", (unsigned int) page_table, page_table);
      printf("PTE offset %ld(0x%03x)\n", PTE32_INDEX(fault_address), PTE32_INDEX(fault_address));
      printf("PTE index %ld(0x%03x)\n",  PTE32_INDEX(fault_address) * PTE_SIZE_BYTES, PTE32_INDEX(fault_address) * PTE_SIZE_BYTES);
      printf("PTE virtual address %ld(0x%08x)\n", (unsigned int) page_table + PTE32_INDEX(fault_address), page_table + PTE32_INDEX(fault_address));

      printf("PAGE table base address %ld(0x%08x)\n", pte->physical_address, pte->physical_address);
      printf("PAGE offset %ld(0x%08x)\n", FRAME_INDEX(fault_address), FRAME_INDEX(fault_address));
      printf("PAGE index %ld(0x%08x)\n", FRAME_INDEX(fault_address) * PTE_SIZE_BYTES, FRAME_INDEX(fault_address) * PTE_SIZE_BYTES);
      printf("PAGE physical address %ld(0x%08x)\n", pte->physical_address + FRAME_INDEX(fault_address), pte->physical_address  + FRAME_INDEX(fault_address));
   #endif

   return ((struct frame_struct *) pte);
}

int fifo()
{
   current_index++;
   current_index = current_index % numframes;            
   return (current_index);
}

/* Opt algorithm implementation */
int opt_alg(struct frame_struct *head, int index, unsigned int *address_array, unsigned int accesses)
{
   struct frame_struct *curr = head;
   int i=0, j;

   //If page that will never be used again is present, evict it
   if (p2e_count > 0)
   {
      p2e_count--;
      return pages_to_evict[p2e_count];
   }

   while (curr->next)
   {
      //Fill empty frames first
      if (!curr->pte_pointer)
      {
         return curr->frame_number;
      }

      //If last calculated count for a frame has passed, reset it to -1
      if (counts[i] < index)
      {
         counts[i] = -1;
      }

      //If the count for a page has not been calculated, calculate it
      if (counts[i] == -1)
      {
         for (j=index; j < accesses; j++)
         {
            if (curr->virtual_address == address_array[j])
            {
               counts[i] = j; //Set count to when page will next be used
               break;
            }
            else if (j+1 == accesses) //Page will no longer be used in the program
            {
               counts[i] = -1;
               pages_to_evict[p2e_count] = curr->frame_number; //Add page to the stack of pages to be evicted
               p2e_count++;
            }
         }
      }
      curr = curr->next;
      i++;
   }

   //If page that will never be used again is present, evict it
   if (p2e_count > 0)
   {
      p2e_count--;
      return pages_to_evict[p2e_count];
   }

   //Loop through counts array to find page that will be used furthest in future and evict it
   int longest = -1;
   int evict_index = 0;
   for (i=0; i < numframes; i++)
   {
      if (counts[i] > longest)
      {
         evict_index = i;
         longest = counts[i];
      }
   }
   counts[evict_index] = -1; //Clear evicted page's count

   return evict_index;
}

/* Clock algorithm implementation */
int clock_alg(struct frame_struct *head)
{
   int page2evict = 0;

   while(1)
   {
      //Clock is pointing to an empty frame
      if (!clock_pointer->pte_pointer)
      {
         page2evict = clock_pointer->frame_number;

         if (clock_pointer->next)
         {
            clock_pointer = clock_pointer->next;
         }
         else
         {
            clock_pointer = head;
         }
         return page2evict;
      }
      //Clock is pointing to a page that is not referenced
      else if (!clock_pointer->pte_pointer->referenced)
      {
         page2evict = clock_pointer->frame_number;

         if (clock_pointer->next)
         {
            clock_pointer = clock_pointer->next;
         }
         else
         {
            clock_pointer = head;
         }
         return page2evict;
      }
      //Clock is pointing to a referenced page (Second Chance)
      else
      {
         clock_pointer->pte_pointer->referenced = 0;

         if (clock_pointer->next)
         {
            clock_pointer = clock_pointer->next;
         }
         else
         {
            clock_pointer = head;
         }
      }
   }
}

/* NRU algorithm implementation */
int nru_alg(struct frame_struct *head)
{
   struct frame_struct *curr = head;

   int page2evict = 0;
   int *group0 = malloc(sizeof(int)*numframes); //!Referenced && !Dirty
   int *group1 = malloc(sizeof(int)*numframes); //!Referenced && Dirty
   int *group2 = malloc(sizeof(int)*numframes); //Referenced && !Dirty
   int *group3 = malloc(sizeof(int)*numframes); //Referenced && Dirty
   int count_g0 = 0;
   int count_g1 = 0;
   int count_g2 = 0;
   int count_g3 = 0;

   //Loop through frames and add to groups based on Referenced & Dirty bits
   while(curr->next)
   {
      if (curr->pte_pointer)
      {
         if (!curr->pte_pointer->referenced && !curr->pte_pointer->dirty)
         {
            group0[count_g0] = curr->frame_number;
            count_g0++;
         }
         else if (!curr->pte_pointer->referenced)
         {
            group1[count_g1] = curr->frame_number;
            count_g1++;
         }
         else if (!curr->pte_pointer->dirty)
         {
            group2[count_g2] = curr->frame_number;
            count_g2++;
         }
         else
         {
            group3[count_g3] = curr->frame_number;
            count_g3++;
         }
      }
      else
      {
         return curr->frame_number; //If frame is empty return its number
      }
      curr = curr->next;
   }

   //Check groups by order of precedence in NRU for frames and chose random frame from group
   if (count_g0 > 0)
   {
      page2evict = group0[(rand() % count_g0)];
   }
   else if (count_g1 > 0)
   {
      page2evict = group1[(rand() % count_g1)];
   }
   else if (count_g2 > 0)
   {
      page2evict = group2[(rand() % count_g2)];
   }
   else if (count_g3 > 0)
   {
      page2evict = group3[(rand() % count_g3)];
   }

   //Free allocated arrays
   free(group0);
   free(group1);
   free(group2);
   free(group3);

   return page2evict;
}

/* Random replacement algorithm implementation */
int rand_alg(struct frame_struct *head)
{
   struct frame_struct *curr = head;

   //Fill empty frames first
   while (curr->next)
   {
      if (!curr->pte_pointer)
      {
         return curr->frame_number;
      }
      else
      {
         curr = curr->next;
      }
   }

   //When all frames full, evict random page
   return (rand() % numframes);
}
