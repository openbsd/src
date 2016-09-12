/*	$OpenBSD: s_significandf.S,v 1.3 2016/09/12 19:47:01 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

ENTRY(significandf)
	XMM_ONE_ARG_FLOAT_PROLOGUE
	flds	ARG_FLOAT_ONE
	fxtract
	fstp	%st(1)
	XMM_FLOAT_EPILOGUE
	ret
END(significandf)
