/*	$OpenBSD: spinlock.h,v 1.3 1999/01/27 04:46:06 imp Exp $	*/

#ifndef _MIPS_SPINLOCK_H_
#define _MIPS_SPINLOCK_H_

#define _SPINLOCK_UNLOCKED	(0)
#define _SPINLOCK_LOCKED	(1)
typedef int _spinlock_lock_t;

#endif /* !_MIPS_SPINLOCK_H_ */
