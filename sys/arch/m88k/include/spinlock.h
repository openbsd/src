/*	$OpenBSD: spinlock.h,v 1.1 2004/04/26 12:34:05 miod Exp $	*/

#ifndef _M88K_SPINLOCK_H_
#define _M88K_SPINLOCK_H_

#define _SPINLOCK_UNLOCKED	(0)
#define _SPINLOCK_LOCKED	(1)
typedef int _spinlock_lock_t;

#endif
