/*	$OpenBSD: e_atan2.S,v 1.4 2016/09/12 19:47:01 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

ENTRY(atan2)
	XMM_TWO_ARG_DOUBLE_PROLOGUE
	fldl	ARG_DOUBLE_ONE
	fldl	ARG_DOUBLE_TWO
	fpatan
	XMM_DOUBLE_EPILOGUE
	ret
END_STD(atan2)
