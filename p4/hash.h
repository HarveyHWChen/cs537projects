#ifndef __hash_h__
#define __hash_h__

#include "spin.h"
#include "list.h"

/** hash table structure **/
typedef struct __hash_t {
  spinlock_t mutex; //Change spinlock_t* mutex to spinlock_t mutex-------------------SHU
  unsigned int numBuckets;
  list_t* buckets; //delete one *
} hash_t;

void Hash_Init(hash_t *hash, int buckets);
void Hash_Insert(hash_t *hash, void *element, unsigned int key);
void Hash_Delete(hash_t *hash, unsigned int key);
void *Hash_Lookup(hash_t *hash, unsigned int key);

#endif
