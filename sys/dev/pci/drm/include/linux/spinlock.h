/* Public domain. */

#ifndef _LINUX_SPINLOCK_H
#define _LINUX_SPINLOCK_H

#include <linux/kernel.h>
#include <linux/spinlock_types.h>
#include <linux/preempt.h>
#include <linux/bottom_half.h>

static inline void
spin_lock_irqsave(struct mutex *mtxp, __unused unsigned long flags)
{
	mtx_enter(mtxp);
}
static inline void
spin_lock_irqsave_nested(struct mutex *mtxp, __unused unsigned long flags,
    __unused int subclass)
{
	mtx_enter(mtxp);
}
static inline void
spin_unlock_irqrestore(struct mutex *mtxp, __unused unsigned long flags)
{
	mtx_leave(mtxp);
}

#define spin_lock(mtxp)			mtx_enter(mtxp)
#define spin_lock_nested(mtxp, l)	mtx_enter(mtxp)
#define spin_unlock(mtxp)		mtx_leave(mtxp)
#define spin_lock_irq(mtxp)		mtx_enter(mtxp)
#define spin_unlock_irq(mtxp)		mtx_leave(mtxp)
#define assert_spin_locked(mtxp)	MUTEX_ASSERT_LOCKED(mtxp)
#define spin_trylock_irq(mtxp)		mtx_enter_try(mtxp)

#define down_read(rwl)			rw_enter_read(rwl)
#define down_read_trylock(rwl)		(rw_enter(rwl, RW_READ | RW_NOSLEEP) == 0)
#define up_read(rwl)			rw_exit_read(rwl)
#define down_write(rwl)			rw_enter_write(rwl)
#define up_write(rwl)			rw_exit_write(rwl)
#define downgrade_write(rwl)		rw_enter(rwl, RW_DOWNGRADE)
#define read_lock(rwl)			rw_enter_read(rwl)
#define read_unlock(rwl)		rw_exit_read(rwl)
#define write_lock(rwl)			rw_enter_write(rwl)
#define write_unlock(rwl)		rw_exit_write(rwl)

#endif
