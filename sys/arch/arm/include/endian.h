/*	$OpenBSD: endian.h,v 1.1 2004/02/01 05:09:49 drahn Exp $	*/

#ifdef __ARMEB__
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif
#include <sys/endian.h>
