/*	$OpenBSD: endian.h,v 1.7 2014/07/12 16:25:08 guenther Exp $	*/

#ifndef _ARM_ENDIAN_H_
#define _ARM_ENDIAN_H_

#define _BYTE_ORDER _LITTLE_ENDIAN
#define	__STRICT_ALIGNMENT

#ifndef __FROM_SYS__ENDIAN
#include <sys/endian.h>
#endif
#endif /* _ARM_ENDIAN_H_ */
