/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: e_remainder.S,v 1.7 2003/07/26 19:24:59 salo Exp $")

ENTRY(__ieee754_remainder)
	XMM_TWO_ARG_DOUBLE_PROLOGUE
	fldl	ARG_DOUBLE_TWO
	fldl	ARG_DOUBLE_ONE
1:	fprem1
	fstsw	%ax
	btw	$10,%ax
	jc	1b
	fstp	%st(1)
	XMM_DOUBLE_EPILOGUE
	ret
