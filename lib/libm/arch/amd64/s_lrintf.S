/*	$OpenBSD: s_lrintf.S,v 1.1 2009/04/10 11:30:33 martynas Exp $	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <machine/asm.h>

ENTRY(lrintf)
	cvtss2si %xmm0, %rax
	ret

