/*	$OpenBSD: spinlock.h,v 1.2 1999/01/26 23:39:29 d Exp $	*/

#ifndef _MIPS_SPINLOCK_H_
#define _MIPS_SPINLOCK_H_

#define _SPINLOCK_UNLOCKED	(0)
#define _SPINLOCK_LOCKED	(1)
typedef int _spinlock_lock_t;

#endif
