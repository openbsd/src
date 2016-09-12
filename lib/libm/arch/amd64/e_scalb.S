/*	$OpenBSD: e_scalb.S,v 1.4 2016/09/12 19:47:01 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

ENTRY(scalb)
	XMM_TWO_ARG_DOUBLE_PROLOGUE
	fldl	ARG_DOUBLE_TWO
	fldl	ARG_DOUBLE_ONE
	fscale
	fstp	%st(1)			/* bug fix for fp stack overflow */
	XMM_DOUBLE_EPILOGUE
	ret
END_NONSTD(scalb)
