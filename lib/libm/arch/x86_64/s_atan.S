/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: s_atan.S,v 1.6 2003/07/26 19:25:00 salo Exp $")

ENTRY(atan)
	XMM_ONE_ARG_DOUBLE_PROLOGUE
	fldl	ARG_DOUBLE_ONE
	fld1
	fpatan
	XMM_DOUBLE_EPILOGUE
	ret
