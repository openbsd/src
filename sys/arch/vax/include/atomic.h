/*	$OpenBSD: atomic.h,v 1.6 2014/03/29 18:09:30 guenther Exp $	*/

/* Public Domain */

#ifndef _MACHINE_ATOMIC_H_
#define _MACHINE_ATOMIC_H_

#if defined(_KERNEL)

#include <machine/mtpr.h>
#include <machine/intr.h>

static __inline void
atomic_setbits_int(volatile unsigned int *uip, unsigned int v)
{
	int s;

	s = splhigh();
	*uip |= v;
	splx(s);
}

static __inline void
atomic_clearbits_int(volatile unsigned int *uip, unsigned int v)
{
	int s;

	s = splhigh();
	*uip &= ~v;
	splx(s);
}

#endif /* defined(_KERNEL) */
#endif /* _MACHINE_ATOMIC_H_ */
