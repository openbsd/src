/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: s_ilogbf.S,v 1.4 1995/10/22 20:32:43 pk Exp $")

ENTRY(ilogbf)
	pushl	%ebp
	movl	%esp,%ebp
	subl	$4,%esp

	flds	8(%ebp)
	fxtract
	fstpl	%st

	fistpl	-4(%ebp)
	movl	-4(%ebp),%eax

	leave
	ret
