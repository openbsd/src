/*	$OpenBSD: hpux_syscalls.c,v 1.12 2004/07/09 21:33:44 mickey Exp $	*/

#if defined(__hppa__)
#include <compat/hpux/hppa/hpux_syscalls.c>
#elif defined(__m68k__)
#include <compat/hpux/m68k/hpux_syscalls.c>
#else
#error COMPILING FOR UNSUPPORTED ARCHITECTURE
#endif
