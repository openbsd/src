/* Public domain. */

#ifndef _LINUX_KTHREAD_H
#define _LINUX_KTHREAD_H

/* both for printf */
#include <sys/types.h>
#include <sys/systm.h>

struct proc *kthread_run(int (*)(void *), void *, const char *);
void 	kthread_park(struct proc *);
void 	kthread_unpark(struct proc *);
int	kthread_should_park(void);
void 	kthread_parkme(void);
void	kthread_stop(struct proc *);
int	kthread_should_stop(void);

#endif
