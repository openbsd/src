/*	$OpenBSD: s_copysignf.S,v 1.7 2018/02/05 04:01:49 deraadt Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

	.section .rodata
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
END_STD(copysignf)
