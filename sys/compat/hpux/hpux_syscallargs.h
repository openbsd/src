/*	$OpenBSD: hpux_syscallargs.h,v 1.14 2004/07/09 21:33:44 mickey Exp $	*/

#if defined(__hppa__)
#include <compat/hpux/hppa/hpux_syscallargs.h>
#elif defined(__m68k__)
#include <compat/hpux/m68k/hpux_syscallargs.h>
#endif
