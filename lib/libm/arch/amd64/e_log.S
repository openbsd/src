/*	$OpenBSD: e_log.S,v 1.4 2016/09/12 19:47:01 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

ENTRY(log)
	XMM_ONE_ARG_DOUBLE_PROLOGUE
	fldln2
	fldl	ARG_DOUBLE_ONE
	fyl2x
	XMM_DOUBLE_EPILOGUE
	ret
END_STD(log)
