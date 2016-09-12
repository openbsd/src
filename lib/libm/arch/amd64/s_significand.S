/*	$OpenBSD: s_significand.S,v 1.3 2016/09/12 19:47:01 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

ENTRY(significand)
	XMM_ONE_ARG_DOUBLE_PROLOGUE
	fldl	ARG_DOUBLE_ONE
	fxtract
	fstp	%st(1)
	XMM_DOUBLE_EPILOGUE
	ret
END(significand)
