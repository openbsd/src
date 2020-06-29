/* Public domain. */

#ifndef _LINUX_TIMEKEEPING_H
#define _LINUX_TIMEKEEPING_H

#define ktime_get_boottime()	ktime_get()
#define get_seconds()		gettime()

static inline time_t
ktime_get_real_seconds(void)
{
	return gettime();
}

static inline struct timeval
ktime_get_real(void)
{
	struct timeval tv;
	getmicrotime(&tv);
	return tv;
}

static inline uint64_t
ktime_get_ns(void)
{
	struct timeval tv = ktime_get();
	return timeval_to_ns(&tv);
}

#endif
