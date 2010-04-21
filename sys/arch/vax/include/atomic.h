/*	$OpenBSD: atomic.h,v 1.4 2010/04/21 03:03:26 deraadt Exp $	*/

/* Public Domain */

#ifndef __VAX_ATOMIC_H__
#define __VAX_ATOMIC_H__

#if defined(_KERNEL)

#include <machine/mtpr.h>
#include <machine/intr.h>

static __inline void
atomic_setbits_int(__volatile unsigned int *uip, unsigned int v)
{
	int s;

	s = splhigh();
	*uip |= v;
	splx(s);
}

static __inline void
atomic_clearbits_int(__volatile unsigned int *uip, unsigned int v)
{
	int s;

	s = splhigh();
	*uip &= ~v;
	splx(s);
}

#endif /* defined(_KERNEL) */
#endif /* __VAX_ATOMIC_H__ */
