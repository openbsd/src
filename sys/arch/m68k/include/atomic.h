/*	$OpenBSD: atomic.h,v 1.3 2007/03/21 05:28:20 miod Exp $	*/

/* Public Domain */

#ifndef __M68K_ATOMIC_H__
#define __M68K_ATOMIC_H__

#if defined(_KERNEL)

static __inline void
atomic_setbits_int(__volatile unsigned int *uip, unsigned int v)
{
	unsigned int witness, old, new;

	do {
		witness = old = *uip;
		new = old | v;
		__asm__ __volatile__ (
			"casl %0, %2, %1" : "+d"(old), "=m"(*uip) : "d"(new));
	} while (old != witness);
}

static __inline void
atomic_clearbits_int(__volatile unsigned int *uip, unsigned int v)
{
	unsigned int witness, old, new;

	do {
		witness = old = *uip;
		new = old & ~v;
		__asm__ __volatile__ (
			"casl %0, %2, %1" : "+d"(old), "=m"(*uip) : "d"(new));
	} while (old != witness);
}

#endif /* defined(_KERNEL) */
#endif /* __M68K_ATOMIC_H__ */
