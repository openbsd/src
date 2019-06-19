/* Public domain. */

#ifndef _LINUX_JIFFIES_H
#define _LINUX_JIFFIES_H

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/limits.h>
#include <sys/kernel.h>

extern volatile unsigned long jiffies;
#define jiffies_64 jiffies /* XXX */
#undef HZ
#define HZ	hz

#define MAX_JIFFY_OFFSET	((INT_MAX >> 1) - 1)

#define time_in_range(x, min, max) ((x) >= (min) && (x) <= (max))

#define jiffies_to_msecs(x)	(((uint64_t)(x)) * 1000 / hz)
#define jiffies_to_usecs(x)	(((uint64_t)(x)) * 1000000 / hz)
#define msecs_to_jiffies(x)	(((uint64_t)(x)) * hz / 1000)
#define usecs_to_jiffies(x)	(((uint64_t)(x)) * hz / 1000000)
#define nsecs_to_jiffies(x)	(((uint64_t)(x)) * hz / 1000000000)
#define nsecs_to_jiffies64(x)	(((uint64_t)(x)) * hz / 1000000000)
#define get_jiffies_64()	jiffies
#define time_after(a,b)		((long)(b) - (long)(a) < 0)
#define time_after32(a,b)	((uint32_t)(b) - (uint32_t)(a) < 0)
#define time_after_eq(a,b)	((long)(b) - (long)(a) <= 0)
#define time_before(a,b)	((long)(a) - (long)(b) < 0)

static inline unsigned long
timespec_to_jiffies(const struct timespec *ts)
{
	long long to_ticks;

	to_ticks = (long long)hz * ts->tv_sec + ts->tv_nsec / (tick * 1000);
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;

	return ((int)to_ticks);
}

#endif
