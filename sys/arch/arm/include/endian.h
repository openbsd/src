/*	$OpenBSD: endian.h,v 1.2 2005/01/20 15:04:54 drahn Exp $	*/

#ifdef __ARMEB__
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif
#define	__STRICT_ALIGNMENT
#include <sys/endian.h>
