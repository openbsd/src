/* $OpenBSD: cdefs.h,v 1.1 2010/09/20 06:32:30 syuu Exp $ */
/* public domain */
#include <mips64/cdefs.h>

#if defined(lint) && !defined(__MIPSEB__)
#define __MIPSEB__
#undef __MIPSEL__
#endif
