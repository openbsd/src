/*	$OpenBSD: endian.h,v 1.6 2018/10/02 21:30:44 naddy Exp $	*/
/*	$NetBSD: endian.h,v 1.4 2000/03/17 00:09:25 mycroft Exp $	*/

/* Written by Manuel Bouyer. Public domain */

#ifndef _SH_ENDIAN_H_
#define	_SH_ENDIAN_H_

#ifndef __FROM_SYS__ENDIAN
#include <sys/_types.h>
#endif

static __inline __uint16_t
__swap16md(__uint16_t _x)
{
	__uint16_t _rv;

	__asm volatile ("swap.b %1,%0" : "=r"(_rv) : "r"(_x));

	 return (_rv);
}

static __inline __uint32_t
__swap32md(__uint32_t _x)
{
	__uint32_t _rv;

	__asm volatile ("swap.b %1,%0; swap.w %0,%0; swap.b %0,%0"
			  : "=r"(_rv) : "r"(_x));

	return (_rv);
}

#define	__swap64md	__swap64gen

/* Tell sys/endian.h we have MD variants of the swap macros.  */
#define __HAVE_MD_SWAP

#ifdef __LITTLE_ENDIAN__
#define	_BYTE_ORDER _LITTLE_ENDIAN
#else
#define	_BYTE_ORDER _BIG_ENDIAN
#endif
#define	__STRICT_ALIGNMENT

#ifndef __FROM_SYS__ENDIAN
#include <sys/endian.h>
#endif

#endif /* !_SH_ENDIAN_H_ */
