/*	$OpenBSD: atomic.h,v 1.4 2007/02/19 17:18:42 deraadt Exp $	*/

/* Public Domain */

#ifndef __ARM_ATOMIC_H__
#define __ARM_ATOMIC_H__

#if defined(_KERNEL)

static __inline void
atomic_setbits_int(__volatile unsigned int *uip, unsigned int v)
{
	*uip |= v;
}

static __inline void
atomic_clearbits_int(__volatile unsigned int *uip, unsigned int v)
{
	*uip &= ~v;
}

#endif /* defined(_KERNEL) */
#endif /* __ARM_ATOMIC_H__ */
