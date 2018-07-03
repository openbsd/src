/*	$OpenBSD: s_significandf.S,v 1.5 2018/07/03 22:43:34 mortimer Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

ENTRY(significandf)
	RETGUARD_SETUP(significandf, r11)
	XMM_ONE_ARG_FLOAT_PROLOGUE
	flds	ARG_FLOAT_ONE
	fxtract
	fstp	%st(1)
	XMM_FLOAT_EPILOGUE
	RETGUARD_CHECK(significandf, r11)
	ret
END(significandf)
	.weak	significandf
