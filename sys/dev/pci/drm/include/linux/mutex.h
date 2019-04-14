/* Public domain. */

#ifndef _LINUX_MUTEX_H
#define _LINUX_MUTEX_H

#include <sys/stdint.h>
#include <sys/rwlock.h>
#include <linux/list.h>
#include <linux/spinlock_types.h>

#define DEFINE_MUTEX(x)		struct rwlock x

#define mutex_lock_interruptible(rwl)	-rw_enter(rwl, RW_WRITE | RW_INTR)
#define mutex_lock(rwl)			rw_enter_write(rwl)
#define mutex_lock_nest_lock(rwl, sub)	rw_enter_write(rwl)
#define mutex_lock_nested(rwl, sub)	rw_enter_write(rwl)
#define mutex_trylock(rwl)		(rw_enter(rwl, RW_WRITE | RW_NOSLEEP) == 0)
#define mutex_unlock(rwl)		rw_exit_write(rwl)
#define mutex_is_locked(rwl)		(rw_status(rwl) == RW_WRITE)
#define mutex_destroy(rwl)

#endif
