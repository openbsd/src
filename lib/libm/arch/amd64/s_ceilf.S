/*	$OpenBSD: s_ceilf.S,v 1.4 2009/04/08 22:30:41 martynas Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(ceilf)
	fstcw	-8(%rsp)
	movw	-8(%rsp),%dx
	orw	$0x0800,%dx
	andw	$0xfbff,%dx
	movw	%dx,-12(%rsp)
	fldcw	-12(%rsp)
	movss	%xmm0,-4(%rsp)
	flds	-4(%rsp)
	frndint
	fldcw	-8(%rsp)
	fstps	-4(%rsp)
	movss	-4(%rsp),%xmm0
	ret
