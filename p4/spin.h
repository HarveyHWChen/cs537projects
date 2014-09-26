#ifndef __spin_h__
#define __spin_h__

#define SPINLOCK_FREE 0
#define SPINLOCK_HELD 1

typedef struct _spinlock_t { 
  volatile unsigned int flag;
} spinlock_t;

//static inline uint xchg(volatile unsigned int *addr, unsigned int newval);

void spinlock_init(spinlock_t* lock);
void spinlock_acquire(spinlock_t* lock);
void spinlock_release(spinlock_t* lock);

#endif
