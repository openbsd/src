/*	$OpenBSD: lock.h,v 1.12 2017/05/25 03:50:10 visa Exp $	*/

/* public domain */

#ifndef _MACHINE_LOCK_H_
#define	_MACHINE_LOCK_H_

#define SPINLOCK_SPIN_HOOK __asm volatile("pause": : :"memory")

#endif /* _MACHINE_LOCK_H_ */
