/*	$OpenBSD: ffs.S,v 1.4 2015/08/31 02:53:56 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include "SYS.h"

ENTRY(ffs)
	bsfl	4(%esp),%eax
	jz	L1	 		/* ZF is set if all bits are 0 */
	incl	%eax			/* bits numbered from 1, not 0 */
	ret

	.align 2
L1:	xorl	%eax,%eax		/* clear result */
	ret
END_WEAK(ffs)
