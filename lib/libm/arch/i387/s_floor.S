/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: s_floor.S,v 1.4 1995/05/09 00:01:59 jtc Exp $")

ENTRY(floor)
	subl	$8,%esp

	fstcw	4(%esp)		/* store fpu control word */
	movw	4(%esp),%dx
	orw	$0x0400,%dx		/* round towards -oo */
	andw	$0xf7ff,%dx
	movw	%dx,(%esp)
	fldcw	(%esp)		/* load modfied control word */

	fldl	12(%esp);		/* round */
	frndint

	fldcw	4(%esp)		/* restore original control word */

	addl	$8,%esp
	ret
