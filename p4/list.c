#include "list.h"
#include "spin.h"
#include <stdio.h>
#include <stdlib.h>


/**
 * Initialize a node
 * @params element: the element of this node
 * @params key: the key of this element
 * @return the pointer points to this node
 */
node_t* Node_Const(void* element, unsigned int key) {
  node_t* newNode=malloc(sizeof(node_t));
  newNode->item = element;
  newNode->key = key;
  newNode->pNext = NULL;
  return newNode;
}

/**
 * Initialize a list
 * @params list: the list to be initialized

 */
void List_Init(list_t *list) {
  if(list == NULL) {
    return;
  }
  spinlock_init(&list->mutex);
  list->pHeader = NULL;
}

/**
 * Insert a node to the head of the list
 * @params list: the list to insert new item in
 * @params element: the element of new node
 * @params key: the key of new node
 */
void List_Insert(list_t *list, void *element, unsigned int key) {
  if(list == NULL) {
    return;
  }
  node_t* newNode = Node_Const(element, key);//malloc(sizeof(node_t));
  //newNode=Node_Const(element, key);
  spinlock_acquire(&list->mutex);
  newNode->pNext = list->pHeader;
  list->pHeader = newNode;
  spinlock_release(&list->mutex);
}

/**
 * Delete a certain code in the list
 * @params list: the list to delete nodes from
 * @params key: the key of the node to be deleted
 */								 
void List_Delete(list_t *list, unsigned int key) {
  if(list == NULL) {
    return;
  }
  spinlock_acquire(&list->mutex);
  //check list is emputy or not-----------Shu
  if(NULL != list->pHeader)
  {
      // check if the header node has identical key
      if(list->pHeader->key == key) {
	// the second node becomes header
	list->pHeader = list->pHeader->pNext;
      } else {
	// if not the first node, look up the list to find corresponding node
	node_t* currNode = list->pHeader;
	while(currNode->pNext!=NULL) {
	  // if found, delete corresponding node
	  if(currNode->pNext->key == key) {
	    currNode->pNext = currNode->pNext->pNext;
	    break;// Add break here----------------Shu
	  }
	  // if not found, proceed to next node
	  currNode = currNode->pNext;
	}
	//if not found, do nothing
      }
   }
  spinlock_release(&list->mutex);
}

/**
 * Look up the list for certain key
 * @params list: the list to kook up
 * @params key: the key of the node to be looking for
 * @return the pointer to the element with identical key
 */

void *List_Lookup(list_t *list, unsigned int key) {
  if(list == NULL) {
    return NULL;
  }
  spinlock_acquire(&list->mutex);
  node_t* currNode = list->pHeader;
  while(currNode != NULL) {
    if(currNode->key == key) {
      //before return unlock--------SHU
      spinlock_release(&list->mutex);
      return currNode->item;
    } else {
      currNode = currNode->pNext;
    }
  }
  spinlock_release(&list->mutex);
  return NULL;
}
