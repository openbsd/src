/* Public domain. */

#ifndef _LINUX_KTHREAD_H
#define _LINUX_KTHREAD_H

/* both for printf */
#include <sys/types.h>
#include <sys/systm.h>

#define kthread_stop(a)		do { printf("%s: stub\n", __func__); } while(0)
#define kthread_park(a)		do { printf("%s: stub\n", __func__); } while(0)
#define kthread_unpark(a)	do { printf("%s: stub\n", __func__); } while(0)
#define kthread_should_stop()	0
#define kthread_should_park()	0
#define kthread_parkme()

#endif
