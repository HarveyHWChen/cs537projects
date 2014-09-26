#ifndef __counter_h__
#define __counter_h__

#include "spin.h"

/** counter structure **/
typedef struct __counter_t {
  spinlock_t mutex;
  int value;
} counter_t;

void Counter_Init(counter_t *c, int value);
int Counter_GetValue(counter_t *c);
void Counter_Increment(counter_t *c);
void Counter_Decrement(counter_t *c);


#endif
