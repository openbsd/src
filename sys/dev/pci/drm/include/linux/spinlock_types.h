/* Public domain. */

#ifndef _LINUX_SPINLOCK_TYPES_H
#define _LINUX_SPINLOCK_TYPES_H

#include <sys/types.h>
#include <sys/mutex.h>
#include <linux/rwlock_types.h>

typedef struct mutex spinlock_t;
#define DEFINE_SPINLOCK(x)	struct mutex x = MUTEX_INITIALIZER(IPL_TTY)

typedef struct raw_spinlock {
} raw_spinlock_t;

#endif
