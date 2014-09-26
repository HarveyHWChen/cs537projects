#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "mem.h"
#include <math.h>
#include <unistd.h>
#include <string.h>

#define MAGIC 1073676287

// the header of allocated memory region and free list node
#pragma pack(push)
#pragma pack(1)
typedef struct __header_t
{
  int size;
  int magic;
} header_t;

typedef struct __node_t
{
  int size;
  struct __node_t* pNext;
} node_t;
#pragma pack(pop)


/** global variables **/
int m_error;
void* start;
int ifDebug = 0;
int init = 0;
int regionSize;
node_t* freeListHeader = NULL;

/** utilities **/
void fillFreePattern(node_t* inNode);
void fillPadding(void* inptr);
int checkFreePattern();
int checkAllPadding();
int checkPadding(header_t* inHeader);
node_t* mergeFreeList();


/** functions **/
/**
 * Initialize memory page.
 * @param sizeOfRegion: the size of memory region;
 * @param debug: if debug mode is used.
 * @return 0 if success; -1 if failed.
 */
int Mem_Init(int sizeOfRegion, int debug)
{
  if(init == 1)
    {
      m_error = E_BAD_ARGS;
      return -1;
    }
  if(sizeOfRegion <= 0)
    {
      m_error = E_BAD_ARGS;
      return -1;
    }
  // if input size is less than 0, or this function is called more than once, return error message
  // round up sizeOfRegin to fit the page size
  int  pageSize = getpagesize();
  //sizeOfRegion = (sizeOfRegion/pageSize) * pageSize;
  regionSize = sizeOfRegion;
  // open the /dev/zero device
  int fd = open("/dev/zero", O_RDWR);
  // sizeOfRegion (in bytes) needs to be evenly divisible by the page size
  start = mmap(NULL, sizeOfRegion, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  // points freeListHeader to the start position of this memory space
  freeListHeader = start;
  freeListHeader->size = sizeOfRegion - sizeof(node_t);
  freeListHeader->pNext = NULL;
  if (start == MAP_FAILED) {
    perror("mmap");
    return -1;
  }
  close(fd);
  // debug mode
  if(debug == 1)
    {
      ifDebug = 1;
      fillFreePattern(freeListHeader);
    }
  init = 1;     
  return 0;
}

/**
 * Allocate memory.
 * @param newSize: the size of memory to be allocated.
 * @return the start address of allocated memory chunk. If failed, return NULL.
 */
void * Mem_Alloc(int newSize)
{
  if(newSize < (sizeof(node_t) - sizeof(header_t)))
    {
      return NULL;
    }
  if(ifDebug == 1)
    {
      if((checkFreePattern() == -1) || (checkAllPadding() == -1))
	{
	  m_error = E_CORRUPT_FREESPACE;
	  return NULL;
	}
    }
  void* newPos = NULL;
  int found = 0;
  // round up newSize to fit into 4-byte
  newSize = newSize / 4 * 4;
  if(ifDebug == 1)
    {
      newSize += 128;
    }
  // check if freeListHeader meets the requirment
  if(freeListHeader->size >= (newSize + sizeof(header_t) - sizeof(node_t)))
    {
      int oriSize = freeListHeader->size;
      // save next node as tmpNext
      node_t* tmpNext = freeListHeader->pNext;
      // check if the last free space is less than sizeof(node_t)
      int newNodeSize =  oriSize - newSize - sizeof(header_t);
      if(newNodeSize <= 0)
	{
	  //if so, delete current freeListHeader, make the next node the new freeListHeader
	  // set up the header of newly allocatd memory chunk
	  header_t* newHeader = (void*) freeListHeader;
	  newHeader->size = oriSize + sizeof(node_t) - sizeof(header_t);
	  if(ifDebug == 1)
	    {
	      newHeader->size -= 128;
	    }
	  newHeader->magic = MAGIC;
	  // modify the freeListHeader
	  freeListHeader = tmpNext;
	  newPos = (void*) newHeader + sizeof(header_t); 
	}
      else
	{
	  // set up the header of newly allocatd memory chunk
	  header_t* newHeader = (void*) freeListHeader;
	  newHeader->size = newSize;
	  if(ifDebug == 1)
	    {
	      newHeader->size -= 128;
	    }
	  newHeader->magic = MAGIC;
	  // set up a new node of updated freeList and link it to the freeListHeader
	  node_t* newNode = (void*)freeListHeader + newSize + sizeof(header_t);
	  newNode->size =  oriSize - newSize - sizeof(header_t);
	  newNode->pNext = tmpNext;
	  freeListHeader = newNode;
	  newPos = (void*) newHeader + sizeof(header_t);
	}
    }
  else
    {
      // if freeListHeader doesn't match requirement
      // traverse free list to find a valid position to allocate memory
      node_t* currPos = freeListHeader;
      while(currPos->pNext != NULL)
	{
	  if(currPos->pNext->size >= (newSize + sizeof(header_t) - sizeof(node_t)))
	    {
	      //currPos = currPos->pNext;
	      found = 1;
	      break;
	    }
	  currPos = currPos->pNext;
	}
      // new currPos points to the node prior to the qualified node in freeList
      if(found == 1)
	{
	  int oriSize = currPos->pNext->size;
	  node_t* tmpNext = currPos->pNext->pNext;
	  // check if the last free space is less than sizeof(node_t)
	  int newNodeSize =  oriSize - newSize - sizeof(header_t);
	  if(newNodeSize <= 0)
	    {
	      //if so, delete current freeListHeader, make the next node the new freeListHeader
	      // set up the header of newly allocatd memory chunk
	      header_t* newHeader = (void*) currPos->pNext;
	      newHeader->size = oriSize + sizeof(node_t) - sizeof(header_t);
	      if(ifDebug == 1)
		{
		  newHeader->size -= 128;
		}
	      newHeader->magic = MAGIC;
	      // modify the freeListHeader
	      currPos->pNext = tmpNext;
	      newPos = (void*) newHeader + sizeof(header_t);
	    }
	  else
	    {
	      // set up the header of newly allocated memory chunk
	      header_t* newHeader = (void*) currPos->pNext;
	      newHeader->size = newSize;
	      if(ifDebug == 1)
		{
		  //newHeader = (void*) currPos->pNext + 64;
		  newHeader->size = newSize - 128;
		}
	      newHeader->magic = MAGIC;
	      // set up a new node of updated freeList and link it to its predecessor
	      node_t* newNode = (void*)currPos->pNext + newSize + sizeof(header_t);
	      newNode->size = oriSize - newSize - sizeof(header_t);
	      newNode->pNext = tmpNext;
	      currPos->pNext = newNode;
	      newPos = (void*) newHeader + sizeof(header_t);
	    }
	}
      else
	{
	  m_error = E_NO_SPACE;
	  return NULL;
	}
    }
  // return allocated memory pointer
  if(ifDebug == 1)
    {
      fillPadding(newPos);
      newPos = (void*)newPos + 64;
    }
  return newPos;
}
  
/**
 * Free memory object that ptr points to. Similiar to free().
 * @param ptr: the point that points to the memory object to be freed.
 * @return 0 if success; -1 if failed; if input pointer is NULL, no operatioin is performed.
 */
int Mem_Free(void* ptr)
{
  // try to keep freeListNodes in order
  int freedSize;
  header_t* header;
  node_t* newNode = ptr - sizeof(header_t);
  if(ifDebug == 1)
    {
      header = ptr - sizeof(header_t) - 64;
      if(checkPadding(header) == -1)
	{
	  m_error = E_PADDING_OVERWRITTEN;
	  return -1;
	}
    }
  else
    {
      header = ptr - sizeof(header_t);
    }
  if(ptr == NULL)
    {
      return 0;
    }
  // verify the sanity of this memory chunk
  if(header->magic != MAGIC)
    {
      m_error = E_BAD_POINTER;
      return -1;
    }
  if(ifDebug == 1)
    {
      newNode = ptr - sizeof(header_t) - 64;
      freedSize = header->size + 128;
    }
  else
    {
      freedSize = header->size;
    }
  // build a new freeList Node
  newNode->size = freedSize + sizeof(header_t) - sizeof(node_t);
  // insert this new node to freeList
  // if insert before freeListHeader
  if(freeListHeader == NULL)
    {
      freeListHeader = newNode;
      freeListHeader->pNext = NULL;
    }
  else
    {
      if(newNode < freeListHeader)
	{
	  newNode->pNext = freeListHeader;
	  freeListHeader = newNode;
	}
      else
	{
	  // else find a place to insert using insertion sort
	  node_t* currNode = freeListHeader;
	  while(currNode->pNext != NULL)
	    {
	      if(currNode->pNext > newNode)
		{
		  break;
		}
	      currNode = currNode->pNext;
	    }
	  newNode->pNext = currNode->pNext;
	  currNode->pNext = newNode;
	}
    }
  newNode = mergeFreeList(newNode);
  if(ifDebug == 1)
    {
      fillFreePattern(newNode);
    }
  return 0;
}

/**
 * Debugging routine. Display the current memory allocation status.
 */
void Mem_Dump()
{
  node_t* currPos = freeListHeader;
  printf("Memory Chunk start at: %p\n", start);
  while(currPos != NULL)
    {
      printf("free memory block at: %p\tlength: %d\n",currPos, currPos->size);
      currPos = currPos->pNext;
    }
}

/**
 * Traverse and find if freeList needs to be merged
 * When there are nodes pointing at adjacent free blocks, those nodes need to be merged.
 * When a node points to a zero-sized free block, this node needs to be deleted.
 * @param the newly added freeList node
 * @return merged freeListNode
 */
node_t* mergeFreeList(void* ptr)
{
  int mergeBefore = 1;
  int mergeAfter = 1;
  header_t* mergePos;
  // if freeList is kept in order
  // traverse freeList to find this position
  node_t* currNode = freeListHeader;
  node_t* thisNode = freeListHeader;
  if(ptr == freeListHeader)
    {
      if(((void*)freeListHeader + freeListHeader->size + sizeof(node_t)) == freeListHeader->pNext)
	{
	  //merge
	  freeListHeader->size += freeListHeader->pNext->size + sizeof(node_t);
	  freeListHeader->pNext = freeListHeader->pNext->pNext;
	}
    }
  else
    {
      while(currNode->pNext != NULL)
	{
	  // locate corresponding node
	  if(currNode->pNext == ptr)
	    {
	      break;
	    }
	  currNode = currNode->pNext;
	}
      thisNode = currNode->pNext; //thisNode == ptr
      node_t* prevNode = currNode;
      if(((void*)prevNode + prevNode->size + sizeof(node_t)) == thisNode)
	{
	  //merge
	  prevNode->size += thisNode->size + sizeof(node_t);
	  prevNode->pNext = thisNode->pNext;
	  thisNode = prevNode;
	}
      if(thisNode->pNext != NULL)
	{
	  node_t* nextNode = thisNode->pNext;
	  if(((void*)thisNode + thisNode->size + sizeof(node_t)) == nextNode)
	    {
	      //merge
	      thisNode->size += nextNode->size + sizeof(node_t);
	      thisNode->pNext = nextNode->pNext;
	    }
	}
    }
  return thisNode;
}


/**
 * when debug mode is on, fill the free space with correct pattern 0xDEADBEEF
 * @param inNode the freeListNode
 */
void fillFreePattern(node_t* inNode)
{
  int* ptr = (void*)inNode + sizeof(node_t);
  while((void*)ptr < ((void*)inNode + inNode->size + sizeof(node_t)))
    {
      *ptr = 0xDEADBEEF;
      ptr++;
    }
}

/**
 * when debug mode is on, check each freeList space if its filled with correct pattern
 * @return -1 if failed, 0 if success
 */
int checkFreePattern()
{
  node_t* currNode = freeListHeader;
  int valid = 0;
  while(currNode != NULL)
    {
      //      int* ptr = (void*)currNode + currNode->size;
      int* ptr = (void*)currNode + sizeof(node_t);
      while((void*)ptr < ((void*)currNode + currNode->size + sizeof(node_t)))
	{
	  if(*ptr != 0xDEADBEEF)
	    {
	      valid = -1;
	      break;
	    }
	  ptr++;
	}
      currNode = currNode->pNext;
    }
  return valid;
}

/**
 * when debug mode is on, fill allocated memory block with padding 0xABCDDCBA
 * under the premise that extra space for 
 * @param inHeader: the header of memory area needs padding
 */
void fillPadding(void* inptr)
{
  header_t* inHeader = inptr - sizeof(header_t);
  if(inHeader->magic != MAGIC)
    return;
  void* startPos = (void*)inHeader + sizeof(header_t);
  void* endPos = (void*)inHeader + + inHeader->size + sizeof(header_t) + 128;
  int* ptr = startPos;
  // fill front padding area
  while((void*)ptr < (startPos + 64))
    {
      *ptr = 0xABCDDCBA;
      ptr++;
    }
  ptr = endPos - 64;
  // fill rear padding area
  while((void*)ptr < endPos)
    {
      *ptr = 0xABCDDCBA;
      ptr++;
    }
}

/**
 * when debug mode is on, check each allocated memory block if it's correctly filled with padding.
 * @return 0 if success, -1 if failed.
 */
int checkAllPadding()
{
  void* currPos = start;
  while(currPos < (start + regionSize))
    {
      header_t* tempHeader = currPos;
      node_t* tempNode = currPos;
      if(tempHeader->magic == MAGIC)
	{
	  // this is a allocated memory block
	  if(checkPadding(tempHeader) == -1)
	    return -1;
	  // move to next memory block
	  currPos += tempHeader->size + sizeof(header_t) + 128;
	}
      else
	{
	  // this is a free space
	  // move to next memory block immediately
	  currPos += tempNode->size + sizeof(node_t);
	}
    }
  return 0;
}

/**
 * when debug mode is on, check this allocated memory block if it's correctly filled with padding.
 * @param inHeader: input memory header
 * @return 0 if success, -1 if failed.
 */
int checkPadding(header_t* inHeader)
{
  //header_t* inHeader = inptr - sizeof(header_t) - 64;
  if(inHeader->magic != MAGIC)
    return -1;
  int valid = 0;
  void* startPos = (void*)inHeader + sizeof(header_t);
  void* endPos = (void*)inHeader + inHeader->size + sizeof(header_t) + 128;
  int* ptr = startPos;
  // check front padding area
  while((void*)ptr < (startPos + 64))
    {
      if(*ptr != 0xABCDDCBA)
	{
	  valid = -1;
	  break;
	}
      ptr++;
    }
  ptr = endPos - 64;
  // check rear padding area
  while((void*)ptr < endPos)
    {
      if(*ptr != 0xABCDDCBA)
	{
	  valid = -1;
	  break;
	}
      ptr++;
    }
  return valid;
}
