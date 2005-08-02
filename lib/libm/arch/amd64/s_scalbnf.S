/*	$OpenBSD: s_scalbnf.S,v 1.2 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

ENTRY(scalbnf)
	XMM_TWO_ARG_FLOAT_PROLOGUE
	fildl	ARG_FLOAT_TWO
	flds	ARG_FLOAT_ONE
	fscale
	fstp	%st(1)		/* bug fix for fp stack overflow */
	XMM_FLOAT_EPILOGUE
	ret
