/*	$OpenBSD: spinlock.h,v 1.3 2011/03/23 16:54:36 pirofti Exp $	*/
 /* Public domain */

#ifndef _MIPS64_SPINLOCK_H_
#define _MIPS64_SPINLOCK_H_

#define _SPINLOCK_UNLOCKED	(0)
#define _SPINLOCK_LOCKED	(1)
typedef int _spinlock_lock_t;

#endif /* !_MIPS64_SPINLOCK_H_ */
