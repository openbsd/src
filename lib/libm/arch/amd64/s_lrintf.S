/*	$OpenBSD: s_lrintf.S,v 1.2 2016/09/12 19:47:01 guenther Exp $	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <machine/asm.h>

ENTRY(lrintf)
	cvtss2si %xmm0, %rax
	ret
END(lrintf)
