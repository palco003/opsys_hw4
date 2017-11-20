#include <stdio.h>
#include "mem.h"

int Mem_Init(int size, int policy){
  if(size <= 0){
		return -1;
	}

	if(mem_policy != -1){
		return -1;
	}

	if(policy != P_BESTFIT && policy != P_WORSTFIT && policy != P_FIRSTFIT){
		return -1;
	}
	else{
		mem_policy = policy;
	}

	// make sure that the region size is page alligned

	int page_size = getpagesize();

	printf("Size of region = %d\n", size);
	printf("Page size = %d\n", page_size);

	int alligned = size % page_size;
	if(alligned != 0){
		size += page_size - alligned;
	}

	printf("Size of region to be allocated = %d\n", size);

	// get the chunk of memory

	int fd = open("/dev/zero", O_RDWR);
	free_head_node =(free_mem *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

	if(free_head_node == NULL){
		return -1;
	}

	close(fd);

	free_head_node->size = size - sizeof(free_mem);
	free_head_node->next = NULL;

	printf("memory chunk address: %8x\nsize = %d\n", free_head_node, free_head_node->size);

	return 0;
}

void* Mem_Alloc(int size){

  return NULL;
}

int Mem_Free(void* ptr){
  return 0;
}

float Mem_GetFragmentation(){

  return 1;
}
