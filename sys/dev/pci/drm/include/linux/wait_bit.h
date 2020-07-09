/* Public domain. */

#ifndef _LINUX_WAIT_BIT_H
#define _LINUX_WAIT_BIT_H

#include <linux/wait.h>

int wait_on_bit(unsigned long *, int, unsigned);
int wait_on_bit_timeout(unsigned long *, int, unsigned, int);
void wake_up_bit(void *, int);
void clear_and_wake_up_bit(int, void *);

wait_queue_head_t *bit_waitqueue(void *, int);

extern wait_queue_head_t var_waitq;

static inline void
wake_up_var(void *var)
{
	wake_up(&var_waitq);
}

#define wait_var_event_interruptible(var, condition)		\
	wait_event_interruptible(var_waitq, condition)

#define wait_var_event_killable(var, condition)			\
	wait_event_killable(var_waitq, condition)

#endif
