/*	$OpenBSD: atomic.h,v 1.9 2014/03/29 18:09:28 guenther Exp $	*/

/* Public Domain */

#ifndef _ARM_ATOMIC_H_
#define _ARM_ATOMIC_H_

#if defined(_KERNEL)

/*
 * on pre-v6 arm processors, it is necessary to disable interrupts if
 * in the kernel and atomic updates are necessary without full mutexes
 */

void atomic_setbits_int(volatile unsigned int *, unsigned int);
void atomic_clearbits_int(volatile unsigned int *, unsigned int);

#endif /* defined(_KERNEL) */
#endif /* _ARM_ATOMIC_H_ */
