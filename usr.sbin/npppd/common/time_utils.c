/*-
 * Copyright 2008 Internet Initiative Japan Inc.
 */
#include <sys/types.h>
#include <time.h>
#include <stdint.h>

#include "time_utils.h"

/**
 * Return the value of system timer in nano seconds.
 * Returns INT64_MIN on error.
 */
int64_t
get_nanotime(void)
{
	struct timespec ts;
	int64_t rval;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
		return INT64_MIN;
	rval = (int64_t)ts.tv_sec * (int64_t)1000000000LL;
	rval = rval + (int64_t)ts.tv_nsec;

	return rval;
}
