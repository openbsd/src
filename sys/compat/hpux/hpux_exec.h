/*	$OpenBSD: hpux_exec.h,v 1.7 2004/07/09 21:33:44 mickey Exp $	*/

#if defined(__hppa__)
#include <compat/hpux/hppa/hpux_exec.h>
#elif defined(__m68k__)
#include <compat/hpux/m68k/hpux_exec.h>
#else
#error COMPILING FOR UNSUPPORTED ARCHITECTURE
#endif
