/*	$OpenBSD: atomic.h,v 1.3 2010/07/01 03:36:12 jsing Exp $	*/

/* Public Domain */

#ifndef __HPPA64_ATOMIC_H__
#define __HPPA64_ATOMIC_H__

#if defined(_KERNEL)

static __inline void
atomic_setbits_int(__volatile unsigned int *uip, unsigned int v)
{
	register_t eiem;

	__asm __volatile("mfctl	%%cr15, %0": "=r" (eiem));
	__asm __volatile("mtctl	%r0, %cr15");
	*uip |= v;
	__asm __volatile("mtctl	%0, %%cr15":: "r" (eiem));
}

static __inline void
atomic_clearbits_int(__volatile unsigned int *uip, unsigned int v)
{
	register_t eiem;

	__asm __volatile("mfctl	%%cr15, %0": "=r" (eiem));
	__asm __volatile("mtctl	%r0, %cr15");
	*uip &= ~v;
	__asm __volatile("mtctl	%0, %%cr15":: "r" (eiem));
}

static __inline void
atomic_setbits_long(__volatile unsigned long *uip, unsigned long v)
{
	register_t eiem;

	__asm __volatile("mfctl	%%cr15, %0": "=r" (eiem));
	__asm __volatile("mtctl	%r0, %cr15");
	*uip |= v;
	__asm __volatile("mtctl	%0, %%cr15":: "r" (eiem));
}

static __inline void
atomic_clearbits_long(__volatile unsigned long *uip, unsigned long v)
{
	register_t eiem;

	__asm __volatile("mfctl	%%cr15, %0": "=r" (eiem));
	__asm __volatile("mtctl	%r0, %cr15");
	*uip &= ~v;
	__asm __volatile("mtctl	%0, %%cr15":: "r" (eiem));
}

#endif /* defined(_KERNEL) */
#endif /* __HPPA64_ATOMIC_H__ */
