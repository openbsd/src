/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: s_logb.S,v 1.6 2003/07/26 19:25:02 salo Exp $")

ENTRY(logb)
	XMM_ONE_ARG_DOUBLE_PROLOGUE
	fldl	ARG_DOUBLE_ONE
	fxtract
	fstp	%st
	XMM_DOUBLE_EPILOGUE
	ret
