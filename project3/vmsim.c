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

int main(int argc, char *argv[])
{
  /*
   * Add sanity check for input arguments
   * Open the memory trace file 
   * and store it in an array
   */
   
   FILE *file;
   if((argc == 6) && (!strcmp(argv[1],"-n")) && (!strcmp(argv[3], "-a")) \
                  && (!strcmp(argv[4], "fifo") || !strcmp(argv[4], "opt") || !strcmp(argv[4], "clock") || !strcmp(argv[4], "nru") || !strcmp(argv[4], "rand")) \
                  && ((!strcmp(argv[5], "gcc.trace")) || (!strcmp(argv[5], "bzip.trace"))))
   {
      numframes = atoi(argv[2]);
      refresh = 100;
      file = fopen(argv[5],"rb");
      if(!file)
      {
         fprintf(stderr, "USAGE: %s -n <numframes> -a <fifo> <tracefile>\n", argv[0]);
         fprintf(stderr, "Error on opening the trace file\n");
         exit(1); 
      }
   }
   else
   {
      fprintf(stderr, "USAGE: %s -n <numframes> -a <fifo> <tracefile>\n", argv[0]);
      exit(1); 
   }

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

   //numaccesses = 100;
   // Main loop to process memory accesses
   for(i = 0; i < numaccesses; i++)
   {
      fault_address = address_array[i];
      mode_type = mode_array[i];
      hit = 0;
    
      //Refresh pages based on refresh rate for nru
      if (!(i % refresh) && strcmp(argv[4], "nru"))
      {
         curr = head;
         while(curr->next)
         {
            curr->pte_pointer->referenced = 0;
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
            page2evict = opt_alg();
         }
         else if(!strcmp(argv[4], "clock"))
         {
            page2evict = clock_alg();
         }
         else if(!strcmp(argv[4], "nru"))
         {
            page2evict = nru_alg();
         }
         else if (!strcmp(argv[4], "rand"))
         {
            page2evict = rand_alg();
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
                     curr->pte_pointer->referenced = 0;

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

int opt_alg()
{

}

int clock_alg()
{

}

int nru_alg()
{

}

/* Random replacement */
int rand_alg()
{
   return (rand() % numframes);
}
