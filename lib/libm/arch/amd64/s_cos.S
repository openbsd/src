/*	$OpenBSD: s_cos.S,v 1.3 2016/09/12 19:47:01 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

ENTRY(cos)
	XMM_ONE_ARG_DOUBLE_PROLOGUE
	fldl	ARG_DOUBLE_ONE
	fcos
	fnstsw	%ax
	andw	$0x400,%ax
	jnz	1f
	XMM_DOUBLE_EPILOGUE
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
	XMM_DOUBLE_EPILOGUE
	ret
END_STD(cos)
