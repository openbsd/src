/*	$OpenBSD: ffs.S,v 1.6 2017/11/29 05:13:57 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include "DEFS.h"

ENTRY(ffs)
	bsfl	4(%esp),%eax
	jz	L1	 		/* ZF is set if all bits are 0 */
	incl	%eax			/* bits numbered from 1, not 0 */
	ret

	.align 2,0xcc
L1:	xorl	%eax,%eax		/* clear result */
	ret
END_WEAK(ffs)
