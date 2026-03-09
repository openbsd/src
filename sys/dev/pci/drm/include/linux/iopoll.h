/* Public domain. */

#ifndef _LINUX_IOPOLL_H
#define _LINUX_IOPOLL_H

/* allowed to sleep */

#define poll_timeout_us(op, cond, sleep_us, timeout_us, sleep_before)	\
({									\
	struct timeval __end, __now, __timeout_tv;			\
	int __timed_out = 0;						\
	uint64_t __timeout_us = timeout_us;				\
	int __sleep_us = sleep_us;					\
									\
	if (__timeout_us) {						\
		microuptime(&__now);					\
		USEC_TO_TIMEVAL(__timeout_us, &__timeout_tv);		\
		timeradd(&__now, &__timeout_tv, &__end);		\
	}								\
									\
	if ((sleep_before) && (__sleep_us))				\
		delay(__sleep_us);					\
									\
	for (;;) {							\
		op;							\
		if (cond)						\
			break;						\
		if (__timeout_us) {					\
			microuptime(&__now);				\
			if (timercmp(&__end, &__now, <=)) {		\
				__timed_out = 1;			\
				break;					\
			}						\
		}							\
		if (__sleep_us)						\
			delay(__sleep_us);				\
	}								\
	(__timed_out) ? -ETIMEDOUT : 0;					\
})

#define readx_poll_timeout(op, addr, val, cond, sleep_us, timeout_us)	\
	poll_timeout_us((val) = (op)(addr), cond, sleep_us, timeout_us, false)

/* not allowed to sleep */
#define poll_timeout_us_atomic(op, cond, sleep_us, timeout_us, sleep_before)	\
	poll_timeout_us(op, cond, sleep_us, timeout_us, sleep_before)

#endif
