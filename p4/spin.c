#include <stdio.h>
#include <stdlib.h>
#include "spin.h"

/**
 * the procedure which calls hardware instructio to change variables atomically
 * @params addr: the address of this variable
 * @params newval: the new value of this variable
 * @return result: the old value of this variable
 */
static inline uint xchg(volatile unsigned int *addr, unsigned int newval) {
  uint result;
  asm volatile("lock; xchgl %0, %1" : "+m" (*addr), "=a" (result) : "1" (newval) : "cc");
  //xchg: exchange the two source operands
  //two outputs: *addr and result
  //"+m", m means the operand is directly from memory, + means read and write
  //"=a": suggest gcc to put the operand value into eax register; `=' means write only
  //one input: newval, "1" means it uses the same constraint as the earlier 1th, i.e., it 
  // will be put into eax and then be overwritten
  //"cc" means the condition register might be altered
  return result;
}

/**
 * Initializa spin lock
 * @params lock: the lock to be initialized
 */
void spinlock_init(spinlock_t* lock){
  lock->flag = SPINLOCK_FREE;
}

/**
 * Acquire a spinlock
 * @params lock: the lock to be acquired
 */
void spinlock_acquire(spinlock_t* lock) {
  // spin wait (do nothing)
  while(xchg(&lock->flag, SPINLOCK_HELD) == SPINLOCK_HELD);
}

/**
 * Release a spin lock
 * @params lock: the lock to be released
 */
void spinlock_release(spinlock_t* lock) {
  // simply release the lock
  xchg(&lock->flag, SPINLOCK_FREE);
}
