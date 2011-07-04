/*	$OpenBSD: exec.h,v 1.1 2011/07/04 23:29:08 pirofti Exp $	*/

/*
 * Written by Paul Irofti <pirofti@openbsd.org>. Public Domain.
 */

#ifndef _IA64_EXEC_H_
#define _IA64_EXEC_H_

#define NATIVE_EXEC_ELF

#define ARCH_ELFSIZE 64

#define _NLIST_DO_ELF
#define _KERN_DO_ELF64

#endif
