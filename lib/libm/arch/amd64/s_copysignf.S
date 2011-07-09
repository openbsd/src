/*	$OpenBSD: s_copysignf.S,v 1.4 2011/07/09 18:00:53 martynas Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

.Lneg:
	.long 0x7fffffff
.Lpos:
	.long 0x80000000

ENTRY(copysignf)
	movss    .Lpos(%rip),%xmm2
	movss    .Lneg(%rip),%xmm3
	pand    %xmm2,%xmm1
	pand    %xmm3,%xmm0
	por     %xmm1,%xmm0
	ret
