/*	$OpenBSD: s_llrint.S,v 1.2 2016/09/12 19:47:01 guenther Exp $	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <machine/asm.h>

ENTRY(llrint)
	cvtsd2si %xmm0, %rax
	ret
END(llrint)
