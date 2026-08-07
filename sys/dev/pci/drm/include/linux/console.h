/* Public domain. */

#ifndef _LINUX_CONSOLE_H
#define _LINUX_CONSOLE_H

#include <linux/types.h>

static inline void
console_lock(void)
{
}

static inline int
console_trylock(void)
{
	return 1;
}

static inline void
console_unlock(void)
{
}

#endif
