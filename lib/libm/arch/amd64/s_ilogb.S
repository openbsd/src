/*	$OpenBSD: s_ilogb.S,v 1.4 2016/09/12 19:47:01 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>
#include "abi.h"

ENTRY(ilogb)
	movsd	%xmm0,-8(%rsp)
	fldl	-8(%rsp)
	fxtract
	fstp	%st
	fistpl	-8(%rsp)
	movl	-8(%rsp),%eax
	ret
END_STD(ilogb)
