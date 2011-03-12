/*	$OpenBSD: endian.h,v 1.3 2011/03/12 04:03:04 guenther Exp $	*/
/*	$NetBSD: endian.h,v 1.4 2000/03/17 00:09:25 mycroft Exp $	*/

/* Written by Manuel Bouyer. Public domain */

#ifndef _MACHINE_ENDIAN_H_
#define	_MACHINE_ENDIAN_H_

#ifdef  __GNUC__

#define	__swap64md	__swap64gen

#define __swap16md(x) __statement({					\
	uint16_t rval;							\
									\
	__asm volatile ("swap.b %1,%0" : "=r"(rval) : "r"(x));		\
									\
	rval;								\
})

#define __swap32md(x) __statement({					\
	uint32_t rval;							\
									\
	__asm volatile ("swap.b %1,%0; swap.w %0,%0; swap.b %0,%0"	\
			  : "=r"(rval) : "r"(x));			\
									\
	rval;								\
})

#define MD_SWAP

#endif /* __GNUC_ */

#ifdef __LITTLE_ENDIAN__
#define	_BYTE_ORDER _LITTLE_ENDIAN
#else
#define	_BYTE_ORDER _BIG_ENDIAN
#endif
#include <sys/endian.h>

#define	__STRICT_ALIGNMENT

#endif /* !_MACHINE_ENDIAN_H_ */
