/*	$OpenBSD: endian.h,v 1.5 2011/03/23 16:54:34 pirofti Exp $	*/

#ifndef _ARM_ENDIAN_H_
#define _ARM_ENDIAN_H_

#ifdef __ARMEB__
#define _BYTE_ORDER _BIG_ENDIAN
#else
#define _BYTE_ORDER _LITTLE_ENDIAN
#endif
#define	__STRICT_ALIGNMENT
#include <sys/endian.h>

#endif /* _ARM_ENDIAN_H_ */
