/* Public domain. */

#ifndef _LINUX_TIMEKEEPING_H
#define _LINUX_TIMEKEEPING_H

#define get_seconds()		time_second
#define getrawmonotonic(x)	nanouptime(x)

#define ktime_mono_to_real(x) (x)
#define ktime_get_real() ktime_get()
#define ktime_get_boottime() ktime_get()

#define do_gettimeofday(tv) getmicrouptime(tv)

static inline int64_t
ktime_get_real_seconds(void)
{
	return ktime_get().tv_sec;
}

#endif
