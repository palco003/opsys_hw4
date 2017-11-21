/*
mem.c
This program is a memory allocator
*/

#include "mem.h"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#define MAGIC_NUMBER 123456
#define true 1
#define false 0
#define FREE_FIRST 0
#define FREE_LAST 1
#define FREE_FREE 2

void merge(void *first_chunk, void *second_chunk);
void merge1(void *chunk);

typedef struct allocated_memory {
	int size;
	int magicNumber;
} allocated_mem;

typedef struct free_memory {
	int size;
	struct free_memory *next;
} free_mem;

static free_mem *free_head_node = NULL;
static int mem_policy = -1;

int Mem_Init(int sizeOfRegion, int policy){

	if(sizeOfRegion <= 0){
		return -1;
	}

	if(mem_policy != -1){
		return -1;
	}

	if(policy != MEM_POLICY_BESTFIT && policy != MEM_POLICY_WORSTFIT && policy != MEM_POLICY_FIRSTFIT){
		return -1;
	}
	else{
		mem_policy = policy;
	}

	// make sure that the region size is page alligned

	int page_size = getpagesize();

	// // printf("Size of region = %d\n", sizeOfRegion);
	// // printf("Page size = %d\n", page_size);

	int alligned = sizeOfRegion % page_size;
	if(alligned != 0){
		sizeOfRegion += page_size - alligned;
	}

	// // printf("Size of region to be allocated = %d\n", sizeOfRegion);

	// get the chunk of memory

	int fd = open("/dev/zero", O_RDWR);
	free_head_node =(free_mem *) mmap(NULL, sizeOfRegion, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

	if(free_head_node == NULL){
		return -1;
	}

	close(fd);

	free_head_node->size = sizeOfRegion - sizeof(free_mem);
	free_head_node->next = NULL;

//	// printf("memory chunk address: %8x\nsize = %d\n", free_head_node, free_head_node->size);

	return 0;
}

void *Mem_Alloc(int size){
	// make sure the information is valid
	if(size <= 0 || mem_policy == -1){
		return NULL;
	}

	// Going to need space to store the header for the memory, so add that size to current size
	// and then make sure that the new number is word alligned
//	// printf("raw size = %d\n", size);
	int needed_size = 4 + sizeof(free_mem);


	int alligned = size % 4;
	size += alligned;
//	// printf("alligned size = %d\n", size);

	// make a previous node that is initally set to NULL and keep it updated, if at the end it is still NULL,
	// then we know that the new head of the free list is *next, otherwise the head of the free list is
	// the first previous.


	free_mem *previous = NULL;
	free_mem *current = free_head_node;
	free_mem *found = NULL;
	free_mem *found_previous = NULL;

	// go through the free list to find the position that we want to allocate
	//// printf("before trying to find the correct node\n");
		while(current != NULL){
			if(mem_policy == MEM_POLICY_FIRSTFIT){
				// printf("current->size = %d\n", current->size);
				// printf("size i'm looking for = %d\n", size);
				if(current->size >= size){
					found_previous = previous;
					found = current;
					break;
				}
			}

			else if(mem_policy == MEM_POLICY_BESTFIT){
				if(current->size == size){
//					// printf("current->size = %d\n", current->size);
//					// printf("size to be allocated = %d\n", size);

					found_previous = previous;
					found = current;
					break;
				}
				else if(current->size > size){
					if(found == NULL){
					//	// printf("current->size = %d\n", current->size);
					//	// printf("size to be allocated = %d\n", size);
						found_previous = previous;
						found = current;
					}
					else if(current->size < found->size){
					//	// printf("current->size = %d\n", current->size);
					//	// printf("size to be allocated = %d\n", size);
						found_previous = previous;
						found = current;
					}
				}

			}

			else if(mem_policy == MEM_POLICY_WORSTFIT){
//				// printf("current->size = %d\n", current->size);
//				// printf("size to be allocated = %d\n", size);
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

//		// printf("found = %016lx\n", found);
//		// printf("found ending address = %016lx\n",(void*) found + (unsigned int) found->size);
		//found = (((void *)found) - ((unsigned int) 7));
		if(found == NULL){
			// printf("Never found the right size and returned null\n");
			return NULL;
		}

		/* at this point we have the node that we want to give back to the user.

		figure out how much space is actually needed out the current chunk and
		update the free list */

		unsigned int unused_space = ((unsigned int)(found->size)) -((unsigned int) size);
//		 // printf("unused space = %d\n", unused_space);
		/* relink the free list to make sure the found node isn't being referenced */


		if(size < found->size && unused_space >= needed_size){

			int address = size + sizeof(free_mem) ;


//			free_mem *node_split = ((free_mem *) found) + address;
			free_mem *node_split =(free_mem*) (((void *) found) + address);
			//// printf("found address = %016lx\n", found);
			//// printf("found->size = %d\n", found->size);
			//// printf("size of header = %d\n", sizeof(free_mem));
			//// printf("new size = %d\n", found->size - size - sizeof(free_mem));
			//// printf("address = %016lx\n", address);
			//// printf("node_split address = %016lx\n",  node_split);
			node_split->size = found->size - size - sizeof(free_mem);
			//// printf("new free node size = %d\n", node_split->size);
			//// printf("good here\n");
			//fflush(stdout);

			node_split->next = found->next;


			if(found_previous == NULL){

				free_head_node = node_split;
			}
			else{
				found_previous->next = node_split;
			}
		}
		else if((found->size > size && unused_space < needed_size)){

			// printf("Unused space is too small to be a new chunk, add it onto the allocated memory\n");
			size += unused_space;
			// printf("new size = %d\n", size);
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

//		// printf("allocated address = %016lx\n",(unsigned int) allocation);
//		// printf("allocated ending address = %016lx\n", (void *)allocation + (unsigned int)allocation->size);
//		checking to see if my makefile still thinks this is up to date


		return (void *) (allocation + 1);

}

int Mem_Free(void *ptr){

	if(ptr == NULL){
		return -1;
	}

	allocated_mem *dealloc = (allocated_mem *) (ptr - 8);


	// if(dealloc->magicNumber != MAGIC_NUMBER){
	// 	return -1;
	// }


	/* known that the pointer is valid and needs to be free'd so replace it
	with a free_mem struct instead of an allocated one */

	free_mem *freed = (free_mem*) dealloc;

//	// printf("freed pointer = %8x\n", ((unsigned long)freed));

	free_mem *previous = NULL;
	free_mem *current = free_head_node;

	free_mem *prev_free_found = NULL;
	free_mem *next_free_found = current;



//	// printf("\n\nBefore Freeing the chunk: %8x\n\n\n", freed);
	Mem_Dump();

	/* Find the previous free chunk and the next free chunk */

	while(current != NULL){
		if(current > freed){
			prev_free_found = previous;
			next_free_found = current;
			break;
		}
		previous = current;
		current = current->next;
	}

	// link the new free chunk into the free list
	freed->next = next_free_found;

	if(prev_free_found == NULL){
		free_head_node = freed;
		merge1(free_head_node);
		//merge(freed, free_head_node);
		//free_head_node = freed;
	}
	else{
		prev_free_found->next = freed;

		merge1(prev_free_found->next);
		merge1(prev_free_found);

		//merge(prev_free_found, freed);
		//merge(freed, next_free_found);
	}
	return 0;

}

void merge1(void *free_chunk){

	free_mem *chunk = (free_mem *)free_chunk;

	if(chunk == NULL || chunk->next == NULL){
		return;
	}

	if(((int) (chunk + 1) + chunk->size) == (int) chunk->next){
		chunk->size += sizeof(free_mem) + chunk->next->size;
		chunk->next = chunk->next->next;
	}
}


void merge(void *first_chunk, void *second_chunk){
	free_mem *first = (free_mem *)first_chunk;
	free_mem *second = (free_mem *)second_chunk;

	// printf("ending address of first = %016lx\n", ((void *)first) + (unsigned int)first->size);
	// printf("starting address of second = %016lx\n", second);

	// printf("ending address = %016lx\n",(void *)second + (unsigned int)second->size);

if(((void *)(((void *)first) + (unsigned int)first->size)) == ((void *) second - (unsigned int)8)){
	first->size += second->size + sizeof(free_mem);
	first->next = second->next;
	// printf("two chunks were merged\n");
	// printf("address = %016lx\n", first);
	// printf("ending address = %016lx\n",(void *)first + (unsigned int)first->size);
	// printf("size of new chunk = %d\n", first->size);
}


}
void Mem_Dump(){

	printf("\n------------------------------\nFREE LIST:\n");
	free_mem* current = free_head_node;
	while(current != NULL){
		printf("address: %8x, total size: %d \n", current, current->size);
		current = current->next;
	}
	printf("------------------------------\n");

}



// #include <stdio.h>
// #include <sys/types.h>
// #include <sys/stat.h>
// #include <fcntl.h>
// #include <sys/mman.h>
// #include "mem.h"
//
// #define MAGIC_NUMBER 123
//
// int global_policy = -1;
//
// typedef struct allocated_memory {
//   int size;
//   int magicNumber;
// } allocated_mem;
//
// typedef struct free_memory {
//   int size;
//   struct free_memory *next;
// } free_mem;
//
// static free_mem *free_head_node = NULL;
//
// int Mem_Init(int size, int policy){
//   if(size <= 0){
//     return -1;
//   }
//
//   if(policy != MEM_POLICY_FIRSTFIT && policy != MEM_POLICY_BESTFIT && policy != MEM_POLICY_WORSTFIT){
//     return -1;
//   } else {
//     global_policy = policy;
//   }
//
//   int page_size = getpagesize();
//
//   // printf("Size of region = %d\n", size);
//   // printf("Page size = %d\n", page_size);
//
//   int aligned = size % page_size;
//   if(aligned != 0){
//     size += page_size - aligned;
//   }
//
//   // printf("Size of region to be allocated = %d\n", size);
//
//   // open the /dev/zero device
//   int fd = open("/dev/zero", O_RDWR);
//   free_head_node = (free_mem *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
//
//   if(free_head_node == NULL){
//     perror("mmap");
//     return -1;
//   }
//
//   close(fd);
//
//   free_head_node->size = size - sizeof(free_mem);
//   free_head_node->next = NULL;
//
//   // printf("memory chunk address: %8x\nsize = %d\n", free_head_node, free_head_node->size);
//
//   return 0;
// }
//
// void* Mem_Alloc(int size){
//   if(size <= 0 || global_policy == -1){
//     return NULL;
//   }
//
//   // Going to need space to store the header for the memory, so add that size to current size
//   // and then make sure that the new number is word aligned
//   // printf("raw size = %d\n", size);
//   int needed_size = 4 + sizeof(free_mem);
//
//   int aligned = size % 4;
//   size += aligned;
//   // printf("aligned size = %d\n", size);
//
//   // make a previous node that is initally set to NULL and keep it updated, if at the end it is still NULL,
//   // then we know that the new head of the free list is *next, otherwise the head of the free list is
//   // the first previous.
//
//   free_mem *previous = NULL;
//   free_mem *current = free_head_node;
//   free_mem *found = NULL;
//   free_mem *found_previous = NULL;
//
//   // go through the free list to find the position that we want to allocate
//   // printf("before trying to find the correct node\n");
//   while(current != NULL){
//     if(global_policy == MEM_POLICY_FIRSTFIT){
//       // printf("current->size = %d\n", current->size);
//       // printf("size i'm looking for = %d\n", size);
//       if(current->size >= size){
//         found_previous = previous;
//         found = current;
//         break;
//       }
//     }
//
//     else if(global_policy == MEM_POLICY_BESTFIT){
//       if(current->size == size){
//         // printf("current->size = %d\n", current->size);
//         // printf("size to be allocated = %d\n", size);
//
//         found_previous = previous;
//         found = current;
//         break;
//       }
//       else if(current->size > size){
//         if(found == NULL){
//           // printf("current->size = %d\n", current->size);
//           // printf("size to be allocated = %d\n", size);
//           found_previous = previous;
//           found = current;
//         }
//         else if(current->size < found->size){
//           // printf("current->size = %d\n", current->size);
//           // printf("size to be allocated = %d\n", size);
//           found_previous = previous;
//           found = current;
//         }
//       }
//
//     }
//
//     else if(global_policy == MEM_POLICY_WORSTFIT){
//       // printf("current->size = %d\n", current->size);
//       // printf("size to be allocated = %d\n", size);
//       if(current->size >= size){
//         if(found == NULL){
//           found_previous = previous;
//           found = current;
//         }
//         else if(current->size > found->size){
//           found_previous = previous;
//           found = current;
//         }
//       }
//     }
//
//     previous = current;
//     current = current->next;
//
//   }
//
//   // printf("found = %016lx\n", found);
//   // printf("found ending address = %016lx\n",(void*) found + (unsigned int) found->size);
//   found = (((void *)found) - ((unsigned int) 7));
//   if(found == NULL){
//     // printf("Never found the right size and returned null\n");
//     return NULL;
//   }
//
//   /* at this point we have the node that we want to give back to the user.
//
//   figure out how much space is actually needed out the current chunk and
//   update the free list */
//
//   unsigned int unused_space = ((unsigned int)(found->size)) -((unsigned int) size);
//   // printf("unused space = %d\n", unused_space);
//   /* relink the free list to make sure the found node isn't being referenced */
//
//
//   if(size < found->size && unused_space >= needed_size){
//
//     int address = size + sizeof(free_mem) ;
//
//
//     free_mem *node_split = ((free_mem *) found) + address;
//     // free_mem *node_split =(free_mem*) (((void *) found) + address);
//     // printf("found address = %016lx\n", found);
//     // printf("found->size = %d\n", found->size);
//     // printf("size of header = %d\n", sizeof(free_mem));
//     // printf("new size = %d\n", found->size - size - sizeof(free_mem));
//     // printf("address = %016lx\n", address);
//     // printf("node_split address = %016lx\n",  node_split);
//     node_split->size = found->size - size - sizeof(free_mem);
//     // printf("new free node size = %d\n", node_split->size);
//     // printf("good here\n");
//     fflush(stdout);
//
//     node_split->next = found->next;
//
//
//     if(found_previous == NULL){
//
//       free_head_node = node_split;
//     }
//     else{
//       found_previous->next = node_split;
//     }
//   }
//   else if((found->size > size && unused_space < needed_size)){
//
//     // printf("Unused space is too small to be a new chunk, add it onto the allocated memory\n");
//     size += unused_space;
//     // printf("new size = %d\n", size);
//     /* there isn't enough unused space to make a chunk out of
//     so give the entire chunk to the user */
//     if(found_previous == NULL){
//       free_head_node = found->next;
//     }
//     else{
//       found_previous->next = found->next;
//     }
//   }
//
//   /* now the found node is out of the free list, there is no one
//   pointing to it. So, change it into an allocated node */
//
//   allocated_mem *allocation = (allocated_mem *) found;
//   allocation->size = size;
//   allocation->magicNumber = MAGIC_NUMBER;
//
//   // printf("allocated address = %016lx\n",(unsigned int) allocation);
//   // printf("allocated ending address = %016lx\n", (void *)allocation + (unsigned int)allocation->size);
//   //		checking to see if my makefile still thinks this is up to date
//
//   return (void *) (allocation + 1);
//   // return NULL;
// }
//
// int Mem_Free(void* ptr){
//   if(ptr == NULL){
//     return -1;
//   }
//
//   allocated_mem *dealloc = (allocated_mem *) (ptr - 8);
//
//
//   if(dealloc->magicNumber != MAGIC_NUMBER){
//     return -1;
//   }
//
//
//   /* known that the pointer is valid and needs to be free'd so replace it
//   with a free_mem struct instead of an allocated one */
//
//   free_mem *freed = (free_mem*) dealloc;
//
//   //	// printf("freed pointer = %8x\n", ((unsigned long)freed));
//
//   free_mem *previous = NULL;
//   free_mem *current = free_head_node;
//
//   free_mem *prev_free_found = NULL;
//   free_mem *next_free_found = current;
//
//
//
//   //	// printf("\n\nBefore Freeing the chunk: %8x\n\n\n", freed);
//   //	Mem_Dump();
//
//   /* Find the previous free chunk and the next free chunk */
//
//   while(current != NULL){
//     if(current > freed){
//       prev_free_found = previous;
//       next_free_found = current;
//       break;
//     }
//     previous = current;
//     current = current->next;
//   }
//
//   // link the new free chunk into the free list
//   freed->next = next_free_found;
//
//   if(prev_free_found == NULL){
//     free_head_node = freed;
//     merge1(free_head_node);
//     //merge(freed, free_head_node);
//     //free_head_node = freed;
//   }
//   else{
//     prev_free_found->next = freed;
//
//     merge1(prev_free_found->next);
//     merge1(prev_free_found);
//
//     //merge(prev_free_found, freed);
//     //merge(freed, next_free_found);
//   }
//
//   return 0;
// }
//
//
// void merge1(void *free_chunk){
//
//   free_mem *chunk = (free_mem *)free_chunk;
//
//   if(chunk == NULL || chunk->next == NULL){
//     return;
//   }
//
//   if(((int) (chunk + 1) + chunk->size) == (int) chunk->next){
//     chunk->size += sizeof(free_mem) + chunk->next->size;
//     chunk->next = chunk->next->next;
//   }
// }
//
//
// void merge(void *first_chunk, void *second_chunk){
//   free_mem *first = (free_mem *)first_chunk;
//   free_mem *second = (free_mem *)second_chunk;
//
//   // printf("ending address of first = %016lx\n", ((void *)first) + (unsigned int)first->size);
//   // printf("starting address of second = %016lx\n", second);
//
//   // printf("ending address = %016lx\n",(void *)second + (unsigned int)second->size);
//
//   if(((void *)(((void *)first) + (unsigned int)first->size)) == ((void *) second - (unsigned int)8)){
//     first->size += second->size + sizeof(free_mem);
//     first->next = second->next;
//     // printf("two chunks were merged\n");
//     // printf("address = %016lx\n", first);
//     // printf("ending address = %016lx\n",(void *)first + (unsigned int)first->size);
//     // printf("size of new chunk = %d\n", first->size);
//   }
//
//
// }
// void Mem_Dump(){
//
//   // printf("------------------------------\nFREE LIST:\n");
//   free_mem* current = free_head_node;
//   while(current != NULL){
//     // printf("address: %8x, total size: %d \n", current, current->size);
//     current = current->next;
//   }
//   // printf("------------------------------\n");
//
// }
//
//
float Mem_GetFragmentation(){

  return 1;
}
