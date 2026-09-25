/* Public domain. */

#ifndef _LINUX_DELAY_H
#define _LINUX_DELAY_H

#include <sys/param.h>
#include <sys/systm.h>

static inline void
udelay(unsigned long usecs)
{
	DELAY(usecs);
}

static inline void
ndelay(unsigned long nsecs)
{
	DELAY(MAX(nsecs / 1000, 1));
}

static inline void
usleep_range(unsigned long min, unsigned long max)
{
	tsleep_nsec(&nowake, PWAIT, "usleep", USEC_TO_NSEC(min));
}

/* XXX assumes state is TASK_UNINTERRUPTIBLE */
static inline void
usleep_range_state(unsigned long min, unsigned long max, unsigned int state)
{
	usleep_range(min, max);
}

static inline void
mdelay(unsigned long msecs)
{
	int loops = msecs;
	while (loops--)
		DELAY(1000);
}

static inline void
drm_msleep(unsigned int msecs)
{
	tsleep_nsec(&nowake, PWAIT, "drmmsleep", MSEC_TO_NSEC(msecs));
}

static inline void
fsleep(unsigned long usecs)
{
	if (usecs <= 10)
		DELAY(usecs);
	else
		tsleep_nsec(&nowake, PWAIT, "fsleep", USEC_TO_NSEC(usecs));
}

static inline unsigned int
msleep_interruptible(unsigned int msecs)
{
	int r = tsleep_nsec(&nowake, PWAIT|PCATCH, "msleepi",
	    MSEC_TO_NSEC(msecs));
	if (r == EINTR)
		return 1;
	return 0;
}

#endif
