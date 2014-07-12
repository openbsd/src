/*	$OpenBSD: endian.h,v 1.2 2014/07/12 16:25:08 guenther Exp $	*/

/*
 * Written by Paul Irofti <pirofti@openbsd.org>. Public Domain.
 */

#ifndef _MACHINE_ENDIAN_H_
#define _MACHINE_ENDIAN_H_

#define _BYTE_ORDER _LITTLE_ENDIAN
#define __STRICT_ALIGNMENT

#ifndef __FROM_SYS__ENDIAN
#include <sys/endian.h>
#endif

#endif /* _MACHINE_ENDIAN_H_ */
