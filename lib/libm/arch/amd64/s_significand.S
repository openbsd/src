/*	$OpenBSD: s_significand.S,v 1.5 2018/07/03 22:43:34 mortimer Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

ENTRY(significand)
	RETGUARD_SETUP(significand, r11)
	XMM_ONE_ARG_DOUBLE_PROLOGUE
	fldl	ARG_DOUBLE_ONE
	fxtract
	fstp	%st(1)
	XMM_DOUBLE_EPILOGUE
	RETGUARD_CHECK(significand, r11)
	ret
END(significand)
	.weak	significand
