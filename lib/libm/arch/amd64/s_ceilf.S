/*	$OpenBSD: s_ceilf.S,v 1.3 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(ceilf)
#ifdef __i386__
	pushl	%ebp
	movl	%esp,%ebp
	subl	$8,%esp

	fstcw	-4(%ebp)		/* store fpu control word */
	movw	-4(%ebp),%dx
	orw	$0x0800,%dx		/* round towards +oo */
	andw	$0xfbff,%dx
	movw	%dx,-8(%ebp)
	fldcw	-8(%ebp)		/* load modfied control word */

	flds	8(%ebp);		/* round */
	frndint

	fldcw	-4(%ebp)		/* restore original control word */

	leave
#else
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
#endif
	ret
