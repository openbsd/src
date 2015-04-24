/*	$OpenBSD: atomic.h,v 1.9 2015/04/24 15:26:22 mpi Exp $	*/

/* Public Domain */

#ifndef _POWERPC_ATOMIC_H_
#define _POWERPC_ATOMIC_H_

#if defined(_KERNEL)

static __inline void
atomic_setbits_int(volatile unsigned int *uip, unsigned int v)
{
	unsigned int tmp;

	__asm volatile (
	    "1:	lwarx	%0, 0, %2	\n"
	    "	or	%0, %1, %0	\n"
	    "	stwcx.	%0, 0, %2	\n"
	    "	bne-	1b		\n"
	    "	sync" : "=&r" (tmp) : "r" (v), "r" (uip) : "cc", "memory");
}

static __inline void
atomic_clearbits_int(volatile unsigned int *uip, unsigned int v)
{
	unsigned int tmp;

	__asm volatile (
	    "1:	lwarx	%0, 0, %2	\n"
	    "	andc	%0, %0, %1	\n"
	    "	stwcx.	%0, 0, %2	\n"
	    "	bne-	1b		\n"
	    "	sync" : "=&r" (tmp) : "r" (v), "r" (uip) : "cc", "memory");
}

#define __membar(_f) do { __asm __volatile(_f ::: "memory"); } while (0)

#ifdef MULTIPROCESSOR
#define membar_enter()		__membar("isync")
#define membar_exit()		__membar("sync")
#define membar_producer()	__membar("sync")
#define membar_consumer()	__membar("isync")
#define membar_sync()		__membar("sync")
#else
#define membar_enter()		__membar("")
#define membar_exit()		__membar("")
#define membar_producer()	__membar("")
#define membar_consumer()	__membar("")
#define membar_sync()		__membar("")
#endif

#endif /* defined(_KERNEL) */
#endif /* _POWERPC_ATOMIC_H_ */
