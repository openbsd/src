/*	$OpenBSD: endian.h,v 1.1 2011/07/04 23:29:08 pirofti Exp $	*/

/*
 * Written by Paul Irofti <pirofti@openbsd.org>. Public Domain.
 */

#ifndef _MACHINE_ENDIAN_H_
#define _MACHINE_ENDIAN_H_

#define __STRICT_ALIGNMENT

#define _BYTE_ORDER _LITTLE_ENDIAN
#include <sys/endian.h>

#endif /* _MACHINE_ENDIAN_H_ */
