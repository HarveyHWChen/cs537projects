#include "counter.h"
#include "spin.h"
#include <stdio.h>

/**
 *Add & to all the c->mutex
 */

/**
 * Initialize counter
 * @params c: the counter to be initialized
 * @params value: the initial value of counter c
 */
void Counter_Init(counter_t* c, int value) {
  if(c == NULL) {
    return;
  }
  spinlock_init(&c->mutex);
  c->value = value;
}

/**
 * Get current value of a counter
 * @params c: the counter to get value from
 * @return the value of counter c
 */
int Counter_GetValue(counter_t* c) {
  // we inteded to check the validity of input pointer c
  // however for a Integer return value, we could not simply return NULL when an error presents
  // so we don't know what to do -_-#
  /*if(c == NULL) {
    return NULL;
    }*/
  spinlock_acquire(&c->mutex);
  int cValue = c->value;
  spinlock_release(&c->mutex);
  return cValue;
}

/**
 * Increase counter value by 1
 * @params c: the counter to be increased
 */
void Counter_Increment(counter_t* c) {
  if(c == NULL) {
    return;
  }
  spinlock_acquire(&c->mutex);
  c->value += 1;
  spinlock_release(&c->mutex);
}

/**
 * Decrease counter value by 1
 * @params c: the counter to be decreased
 */
void Counter_Decrement(counter_t* c) {
  if(c == NULL) {
    return;
  }
  spinlock_acquire(&c->mutex);
  c->value -= 1;
  spinlock_release(&c->mutex);
}
