/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: s_cos.S,v 1.5 1995/05/08 23:54:00 jtc Exp $")

ENTRY(cos)
	fldl	4(%esp)
	fcos
	fnstsw	%ax
	andw	$0x400,%ax
	jnz	1f
	ret	
1:	fldpi
	fadd	%st(0)
	fxch	%st(1)
2:	fprem1
	fnstsw	%ax
	andw	$0x400,%ax
	jnz	2b
	fstp	%st(1)
	fcos
	ret
