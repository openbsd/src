/* Public domain. */

#ifndef _LINUX_HRTIMER_H
#define _LINUX_HRTIMER_H

#include <linux/types.h>
#include <linux/rbtree.h>

#include <sys/timeout.h>

enum hrtimer_restart { HRTIMER_NORESTART, HRTIMER_RESTART };
struct hrtimer {
	enum hrtimer_restart	(*function)(struct hrtimer *);
};

#define HRTIMER_MODE_REL	1

#define hrtimer_cancel(x)		timeout_del_barrier(x)
#define hrtimer_try_to_cancel(x)	timeout_del(x)	/* XXX ret -1 if running */
#define hrtimer_active(x)		timeout_pending(x)

u64 hrtimer_forward_now(struct timeout *, ktime_t);

#endif
