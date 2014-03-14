/*	$OpenBSD: endian.h,v 1.4 2014/03/14 10:47:21 dlg Exp $	*/

#ifndef _MACHINE_ENDIAN_H_
#define _MACHINE_ENDIAN_H_

#define	_BYTE_ORDER _BIG_ENDIAN

#ifdef _KERNEL

#define ASI_P_L	0x88

static inline __uint16_t
__mswap16(volatile __uint16_t *m)
{
	__uint16_t v;

	__asm __volatile("lduha [%1] %2, %0 ! %3"
	    : "=r" (v)
	    : "r" (m), "n" (ASI_P_L), "m" (*m));

	return (v);
}

static inline __uint32_t
__mswap32(volatile __uint32_t *m)
{
	__uint32_t v;

	__asm __volatile("lduwa [%1] %2, %0 ! %3"
	    : "=r" (v)
	    : "r" (m), "n" (ASI_P_L), "m" (*m));

	return (v);
}

static inline __uint64_t
__mswap64(volatile __uint64_t *m)
{
	__uint64_t v;

	__asm __volatile("ldxa [%1] %2, %0 ! %3"
	    : "=r" (v)
	    : "r" (m), "n" (ASI_P_L), "m" (*m));

	return (v);
}

static inline void
__swapm16(volatile __uint16_t *m, __uint16_t v)
{
	__asm __volatile("stha %1, [%2] %3 ! %0"
	    : "=m" (*m)
	    : "r" (v), "r" (m), "n" (ASI_P_L));
}

static inline void
__swapm32(volatile __uint32_t *m, __uint32_t v)
{
	__asm __volatile("stwa %1, [%2] %3 ! %0"
	    : "=m" (*m)
	    : "r" (v), "r" (m), "n" (ASI_P_L));
}

static inline void
__swapm64(volatile __uint64_t *m, __uint64_t v)
{
	__asm __volatile("stxa %1, [%2] %3 ! %0"
	    : "=m" (*m)
	    : "r" (v), "r" (m), "n" (ASI_P_L));
}

#undef ASI_P_L

#define MD_SWAPIO

#endif  /* _KERNEL */

#include <sys/endian.h>

#define __STRICT_ALIGNMENT

#endif /* _MACHINE_ENDIAN_H_ */
