/*	$OpenBSD: atomic.h,v 1.4 2011/03/23 16:54:35 pirofti Exp $	*/

/* Public Domain */

#ifndef _M68K_ATOMIC_H_
#define _M68K_ATOMIC_H_

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
#endif /* _M68K_ATOMIC_H_ */
