/*	$OpenBSD: spinlock.h,v 1.1 2004/08/06 20:56:02 pefo Exp $	*/

#ifndef _MIPS_SPINLOCK_H_
#define _MIPS_SPINLOCK_H_

#define _SPINLOCK_UNLOCKED	(0)
#define _SPINLOCK_LOCKED	(1)
typedef int _spinlock_lock_t;

#endif /* !_MIPS_SPINLOCK_H_ */
