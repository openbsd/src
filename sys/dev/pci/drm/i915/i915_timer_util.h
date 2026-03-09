/* SPDX-License-Identifier: MIT */
/* Copyright © 2025 Intel Corporation */

#ifndef __I915_TIMER_UTIL_H__
#define __I915_TIMER_UTIL_H__

#include <linux/timer.h>
#include <asm/rwonce.h>

void cancel_timer(struct timeout *t);
void set_timer_ms(struct timeout *t, unsigned long timeout);

static inline bool timer_active(const struct timeout *t)
{
#ifdef __linux__
	return READ_ONCE(t->expires);
#else
	return READ_ONCE(t->to_time);
#endif
}

static inline bool timer_expired(const struct timeout *t)
{
	return timer_active(t) && !timer_pending(t);
}

#endif /* __I915_TIMER_UTIL_H__ */
