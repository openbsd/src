/*	$OpenBSD: endian.h,v 1.4 2011/03/11 15:17:08 pirofti Exp $	*/

#ifndef _MACHINE_ENDIAN_H_
#define _MACHINE_ENDIAN_H_

#ifdef __ARMEB__
#define _BYTE_ORDER _BIG_ENDIAN
#else
#define _BYTE_ORDER _LITTLE_ENDIAN
#endif
#define	__STRICT_ALIGNMENT
#include <sys/endian.h>

#endif /* _MACHINE_ENDIAN_H_ */
