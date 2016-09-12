/*	$OpenBSD: s_ceilf.S,v 1.4 2016/09/12 19:47:02 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(ceilf)
	subl	$8,%esp

	fstcw	4(%esp)		/* store fpu control word */
	movw	4(%esp),%dx
	orw	$0x0800,%dx		/* round towards +oo */
	andw	$0xfbff,%dx
	movw	%dx,(%esp)
	fldcw	(%esp)		/* load modfied control word */

	flds	12(%esp);		/* round */
	frndint

	fldcw	4(%esp)		/* restore original control word */

	addl	$8,%esp
	ret
END(ceilf)
