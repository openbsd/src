/* Public domain. */

#ifndef _LINUX_FAULT_INJECT_H
#define _LINUX_FAULT_INJECT_H

#include <linux/types.h>

static inline bool
should_fail(void *v, ssize_t sz)
{
	return false;
}

#endif
