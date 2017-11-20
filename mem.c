#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "mem.h"

int global_policy = -1;

typedef struct allocated_memory {
  int size;
  int magicNumber;
} allocated_mem;

typedef struct free_memory {
  int size;
  struct free_memory *next;
} free_mem;

static free_mem *free_head_node = NULL;

int Mem_Init(int size, int policy){
  if(size <= 0){
    return -1;
  }

  if(policy != MEM_POLICY_FIRSTFIT && policy != MEM_POLICY_BESTFIT && policy != MEM_POLICY_WORSTFIT){
    return -1;
  } else {
    global_policy = policy;
  }

  int page_size = getpagesize();

  printf("Size of region = %d\n", size);
  printf("Page size = %d\n", page_size);

  int aligned = size % page_size;
  if(aligned != 0){
    size += page_size - aligned;
  }

  printf("Size of region to be allocated = %d\n", size);

  // open the /dev/zero device
  int fd = open("/dev/zero", O_RDWR);
  free_head_node = (free_mem *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

  if(free_head_node == NULL){
    perror("mmap");
    return -1;
  }

  close(fd);

  free_head_node->size = size - sizeof(free_mem);
  free_head_node->next = NULL;

  printf("memory chunk address: %8x\nsize = %d\n", free_head_node, free_head_node->size);

  return 0;
}

void* Mem_Alloc(int size){
  if(size <= 0 || global_policy == -1){
    return NULL;
  }

  // Going to need space to store the header for the memory, so add that size to current size
  // and then make sure that the new number is word aligned
  //	printf("raw size = %d\n", size);
  int needed_size = 4 + sizeof(free_mem);

  int aligned = size % 4;
  size += aligned;
  printf("aligned size = %d\n", size);

  // make a previous node that is initally set to NULL and keep it updated, if at the end it is still NULL,
  // then we know that the new head of the free list is *next, otherwise the head of the free list is
  // the first previous.

  free_mem *previous = NULL;
  free_mem *current = free_head_node;
  free_mem *found = NULL;
  free_mem *found_previous = NULL;

  // go through the free list to find the position that we want to allocate
  printf("before trying to find the correct node\n");
  while(current != NULL){
    if(global_policy == MEM_POLICY_FIRSTFIT){
      printf("current->size = %d\n", current->size);
      printf("size i'm looking for = %d\n", size);
      if(current->size >= size){
        found_previous = previous;
        found = current;
        break;
      }
    }

    else if(global_policy == MEM_POLICY_BESTFIT){
      if(current->size == size){
        printf("current->size = %d\n", current->size);
        printf("size to be allocated = %d\n", size);

        found_previous = previous;
        found = current;
        break;
      }
      else if(current->size > size){
        if(found == NULL){
          printf("current->size = %d\n", current->size);
          printf("size to be allocated = %d\n", size);
          found_previous = previous;
          found = current;
        }
        else if(current->size < found->size){
          printf("current->size = %d\n", current->size);
          printf("size to be allocated = %d\n", size);
          found_previous = previous;
          found = current;
        }
      }

    }

    else if(global_policy == MEM_POLICY_WORSTFIT){
      printf("current->size = %d\n", current->size);
      printf("size to be allocated = %d\n", size);
      if(current->size >= size){
        if(found == NULL){
          found_previous = previous;
          found = current;
        }
        else if(current->size > found->size){
          found_previous = previous;
          found = current;
        }
      }
    }

    previous = current;
    current = current->next;

  }

  printf("found = %016lx\n", found);
  printf("found ending address = %016lx\n",(void*) found + (unsigned int) found->size);
  found = (((void *)found) - ((unsigned int) 7));
  if(found == NULL){
    printf("Never found the right size and returned null\n");
    return NULL;
  }

  /* at this point we have the node that we want to give back to the user.

  figure out how much space is actually needed out the current chunk and
  update the free list */

  unsigned int unused_space = ((unsigned int)(found->size)) -((unsigned int) size);
  printf("unused space = %d\n", unused_space);
  /* relink the free list to make sure the found node isn't being referenced */


  if(size < found->size && unused_space >= needed_size){

    int address = size + sizeof(free_mem) ;


    free_mem *node_split = ((free_mem *) found) + address;
    free_mem *node_split =(free_mem*) (((void *) found) + address);
    printf("found address = %016lx\n", found);
    printf("found->size = %d\n", found->size);
    printf("size of header = %d\n", sizeof(free_mem));
    printf("new size = %d\n", found->size - size - sizeof(free_mem));
    printf("address = %016lx\n", address);
    printf("node_split address = %016lx\n",  node_split);
    node_split->size = found->size - size - sizeof(free_mem);
    printf("new free node size = %d\n", node_split->size);
    printf("good here\n");
    fflush(stdout);

    node_split->next = found->next;


    if(found_previous == NULL){

      free_head_node = node_split;
    }
    else{
      found_previous->next = node_split;
    }
  }
  else if((found->size > size && unused_space < needed_size)){

    printf("Unused space is too small to be a new chunk, add it onto the allocated memory\n");
    size += unused_space;
    printf("new size = %d\n", size);
    /* there isn't enough unused space to make a chunk out of
    so give the entire chunk to the user */
    if(found_previous == NULL){
      free_head_node = found->next;
    }
    else{
      found_previous->next = found->next;
    }
  }

  /* now the found node is out of the free list, there is no one
  pointing to it. So, change it into an allocated node */

  allocated_mem *allocation = (allocated_mem *) found;
  allocation->size = size;
  allocation->magicNumber = MAGIC_NUMBER;

  printf("allocated address = %016lx\n",(unsigned int) allocation);
  printf("allocated ending address = %016lx\n", (void *)allocation + (unsigned int)allocation->size);
  //		checking to see if my makefile still thinks this is up to date

  return (void *) (allocation + 1);
  // return NULL;
}

int Mem_Free(void* ptr){
  return 0;
}

float Mem_GetFragmentation(){

  return 1;
}
