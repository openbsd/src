/*	$OpenBSD: atomic.h,v 1.3 2007/03/21 05:28:51 miod Exp $	*/

/* Public Domain */

#ifndef __M88K_ATOMIC_H__
#define __M88K_ATOMIC_H__

#if defined(_KERNEL)

static __inline void
atomic_setbits_int(__volatile unsigned int *uip, unsigned int v)
{
	unsigned int old, new;

	do {
		old = *uip;
		new = old | v;
		__asm__ __volatile__ ("xmem %0, %1, r0" : "+r"(new) : "r"(uip));
	} while (old != new);
}

static __inline void
atomic_clearbits_int(__volatile unsigned int *uip, unsigned int v)
{
	unsigned int old, new;

	do {
		old = *uip;
		new = old & ~v;
		__asm__ __volatile__ ("xmem %0, %1, r0" : "+r"(new) : "r"(uip));
	} while (old != new);
}

#endif /* defined(_KERNEL) */
#endif /* __M88K_ATOMIC_H__ */
