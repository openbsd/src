/*	$OpenBSD: s_copysign.S,v 1.5 2016/09/12 19:47:01 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

.Lpos:
	.quad	0x8000000000000000
.Lneg:
	.quad	0x7fffffffffffffff

ENTRY(copysign)
	movq	.Lpos(%rip),%xmm2
	movq	.Lneg(%rip),%xmm3
	pand	%xmm2,%xmm1
	pand	%xmm3,%xmm0
	por	%xmm1,%xmm0
	ret
END(copysign)
