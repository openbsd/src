/*	$OpenBSD: e_scalb.S,v 1.5 2018/07/03 22:43:34 mortimer Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

ENTRY(scalb)
	RETGUARD_SETUP(scalb, r11)
	XMM_TWO_ARG_DOUBLE_PROLOGUE
	fldl	ARG_DOUBLE_TWO
	fldl	ARG_DOUBLE_ONE
	fscale
	fstp	%st(1)			/* bug fix for fp stack overflow */
	XMM_DOUBLE_EPILOGUE
	RETGUARD_CHECK(scalb, r11)
	ret
END_NONSTD(scalb)
