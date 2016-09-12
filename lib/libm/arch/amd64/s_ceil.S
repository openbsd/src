/*	$OpenBSD: s_ceil.S,v 1.4 2016/09/12 19:47:01 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

ENTRY(ceil)
	fstcw	-12(%rsp)
	movw	-12(%rsp),%dx
	orw	$0x0800,%dx
	andw	$0xfbff,%dx
	movw	%dx,-16(%rsp)
	fldcw	-16(%rsp)
	movsd	%xmm0,-8(%rsp)
	fldl	-8(%rsp)
	frndint
	fldcw	-12(%rsp)
	fstpl	-8(%rsp)
	movsd	-8(%rsp),%xmm0
	ret
END_STD(ceil)
