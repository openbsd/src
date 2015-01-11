/*	$OpenBSD: endian.h,v 1.8 2015/01/11 11:18:36 dlg Exp $	*/

#ifndef _ARM_ENDIAN_H_
#define _ARM_ENDIAN_H_

#ifdef _KERNEL
#ifdef __armv7__
#ifdef __GNUC__

static inline __uint16_t
___swap16md(__uint16_t x)
{
	__uint16_t rv;

	__asm ("rev16 %0, %1" : "=r" (rv) : "r" (x));

	return (rv);
}
#define __swap16md(x) ___swap16md(x)

static inline __uint32_t
___swap32md(__uint32_t x)
{
	__uint32_t rv;

	__asm ("rev %0, %1" : "=r" (rv) : "r" (x));

	return (rv);
}
#define __swap32md(x) ___swap32md(x)

static inline __uint64_t
___swap64md(__uint64_t x)
{
	__uint64_t rv;

	rv = (__uint64_t)__swap32md(x >> 32) |
	    (__uint64_t)__swap32md(x) << 32;

	return (rv);
}
#define __swap64md(x) ___swap64md(x)

/* Tell sys/endian.h we have MD variants of the swap macros.  */
#define __HAVE_MD_SWAP

#endif  /* __GNUC__ */
#endif  /* __armv7__ */
#endif  /* _KERNEL */

#define _BYTE_ORDER _LITTLE_ENDIAN
#define	__STRICT_ALIGNMENT

#ifndef __FROM_SYS__ENDIAN
#include <sys/endian.h>
#endif
#endif /* _ARM_ENDIAN_H_ */
