#ifndef __list_h__
#define __list_h__

#include "spin.h"

/** list structure **/
typedef struct __node_t {
  void* item;
  unsigned int key;
  struct __node_t* pNext;
} node_t;
 
typedef struct __list_t {
  spinlock_t mutex; //Change spinlock_t* mutex to spinlock_t mutex-------------------SHU
  node_t* pHeader;
} list_t;

node_t* Node_Const(void* element, unsigned int key);
void List_Init(list_t *list);
void List_Insert(list_t *list, void *element, unsigned int key);
void List_Delete(list_t *list, unsigned int key);
void *List_Lookup(list_t *list, unsigned int key);

#endif
