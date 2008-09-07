/*	$OpenBSD: e_scalb.S,v 1.3 2008/09/07 20:36:08 martynas Exp $ */
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
