// SPDX-License-Identifier: MIT
/* Copyright © 2025 Intel Corporation */

#include <linux/jiffies.h>

#include "i915_timer_util.h"

void cancel_timer(struct timeout *t)
{
	if (!timer_active(t))
		return;

	timer_delete(t);
	WRITE_ONCE(t->to_time, 0);
}

void set_timer_ms(struct timeout *t, unsigned long timeout)
{
	if (!timeout) {
		cancel_timer(t);
		return;
	}

	timeout = msecs_to_jiffies(timeout);

	/*
	 * Paranoia to make sure the compiler computes the timeout before
	 * loading 'jiffies' as jiffies is volatile and may be updated in
	 * the background by a timer tick. All to reduce the complexity
	 * of the addition and reduce the risk of losing a jiffy.
	 */
	barrier();

	/* Keep t->expires = 0 reserved to indicate a canceled timer. */
	mod_timer(t, jiffies + timeout ?: 1);
}
