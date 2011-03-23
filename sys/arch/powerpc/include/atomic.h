/*	$OpenBSD: atomic.h,v 1.4 2011/03/23 16:54:36 pirofti Exp $	*/

/* Public Domain */

#ifndef _POWERPC_ATOMIC_H_
#define _POWERPC_ATOMIC_H_

#if defined(_KERNEL)

static __inline void
atomic_setbits_int(__volatile unsigned int *uip, unsigned int v)
{
	unsigned int tmp;

	__asm volatile (
	    "1:	lwarx	%0, 0, %2	\n"
	    "	or	%0, %1, %0	\n"
	    "	stwcx.	%0, 0, %2	\n"
	    "	bne-	1b		\n"
	    "	sync" : "=&r" (tmp) : "r" (v), "r" (uip) : "memory");
}

static __inline void
atomic_clearbits_int(__volatile unsigned int *uip, unsigned int v)
{
	unsigned int tmp;

	__asm volatile (
	    "1:	lwarx	%0, 0, %2	\n"
	    "	andc	%0, %0, %1	\n"
	    "	stwcx.	%0, 0, %2	\n"
	    "	bne-	1b		\n"
	    "	sync" : "=&r" (tmp) : "r" (v), "r" (uip) : "memory");
}

#endif /* defined(_KERNEL) */
#endif /* _POWERPC_ATOMIC_H_ */
