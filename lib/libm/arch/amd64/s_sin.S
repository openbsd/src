/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: s_sin.S,v 1.7 2003/07/26 19:25:03 salo Exp $")

ENTRY(sin)
	XMM_ONE_ARG_DOUBLE_PROLOGUE
	fldl	ARG_DOUBLE_ONE
	fsin
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
	fsin
	XMM_DOUBLE_EPILOGUE
	ret
