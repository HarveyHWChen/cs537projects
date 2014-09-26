#include <stdio.h>
#include <stdlib.h>
#include "list.h"
#include "hash.h"
#include "spin.h"


/**
 * Initialize hash table
 * @params hash: the hash table to be initialized
 * @params buckets: the number of buckets in this hash table
 */
void Hash_Init(hash_t* hash, int buckets) {
  if(hash == NULL) {
    return;
  }
  hash->numBuckets = buckets;
  hash->buckets = (list_t*) malloc(buckets*sizeof(list_t));
  int i=0;
  for(;i<buckets;i++)
    {
      List_Init(&hash->buckets[i]);
    }
}

/**
 * Insert an element into hash table
 * @params hash: the hash table to be inserted in
 * @params element: the element to be inserted
 * @params key: the key of element to be inserted
 */
void Hash_Insert(hash_t *hash, void *element, unsigned int key) {
  if(hash == NULL) {
    return;
  }
  // acquire spinlock of this spinlock
  spinlock_acquire(&hash->mutex);
  int hashIndex = key % (hash->numBuckets);
  spinlock_release(&hash->mutex);
  return  List_Insert(&hash->buckets[hashIndex], element, key);
}

/**
 * Delete an element with identical key in hash table
 * @params hash: the hash table to delete elements from
 * @params key: the key to delete
 */
void Hash_Delete(hash_t *hash, unsigned int key) {
  if(hash == NULL) {
    return;
  }  
  spinlock_acquire(&hash->mutex);
  int hashIndex = key % hash->numBuckets;
  List_Delete(&hash->buckets[hashIndex], key);
  spinlock_release(&hash->mutex);
}

/**
 * Look up elements with certain key in hash table
 * @params hash: the hash table to look up elements in
 * @params key: the key to look up
 * @return the pointer points to element with identical key
 */
void *Hash_Lookup(hash_t *hash, unsigned int key) {
  if(hash == NULL) {
    return NULL;
  }  
  spinlock_acquire(&hash->mutex);
  int hashIndex = key % hash->numBuckets;
  void* element = List_Lookup(&hash->buckets[hashIndex], key);
  spinlock_release(&hash->mutex);
  return element;
}
