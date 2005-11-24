/*	$OpenBSD: cdefs.h,v 1.2 2005/11/24 20:46:48 deraadt Exp $ */

/* Use Mips generic include file */

#include <mips64/cdefs.h>

#if defined(lint) && !defined(__MIPSEB__)
#define __MIPSEB__
#undef __MIPSEL__
#endif
