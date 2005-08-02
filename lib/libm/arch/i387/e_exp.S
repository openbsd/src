/*	$OpenBSD: e_exp.S,v 1.5 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

/* e^x = 2^(x * log2(e)) */
ENTRY(__ieee754_exp)
	subl	$8,%esp

	fstcw	4(%esp)		/* store fpu control word */
	movw	4(%esp),%dx
	orw	$0x0180,%dx
	movw	%dx,(%esp)
	fldcw	(%esp)		/* load modfied control word */

	fldl	12(%esp)
	fldl2e
	fmulp				/* x * log2(e) */
	fst	%st(1)
	frndint				/* int(x * log2(e)) */
	fxch	%st(1)
	fsub	%st(1),%st		/* fract(x * log2(e)) */
	f2xm1				/* 2^(fract(x * log2(e))) - 1 */ 
	fld1
	faddp				/* 2^(fract(x * log2(e))) */
	fscale				/* e^x */
	fstp	%st(1)

	fldcw	4(%esp)		/* restore original control word */

	addl	$8,%esp
	ret
