/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: e_log10.S,v 1.6 2003/07/26 19:24:59 salo Exp $")

ENTRY(__ieee754_log10)
	XMM_ONE_ARG_DOUBLE_PROLOGUE
	fldlg2
	fldl	ARG_DOUBLE_ONE
	fyl2x
	XMM_DOUBLE_EPILOGUE
	ret
