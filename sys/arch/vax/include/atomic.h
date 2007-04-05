/*	$OpenBSD: atomic.h,v 1.3 2007/04/05 17:33:50 miod Exp $	*/

/* Public Domain */

#ifndef __VAX_ATOMIC_H__
#define __VAX_ATOMIC_H__

#if defined(_KERNEL)

#include <machine/mtpr.h>

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
