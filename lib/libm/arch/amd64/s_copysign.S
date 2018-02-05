/*	$OpenBSD: s_copysign.S,v 1.7 2018/02/05 04:01:49 deraadt Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

	.section .rodata
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
END_STD(copysign)
