/*	$OpenBSD: e_sqrtl.S,v 1.2 2016/09/12 19:47:01 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>
#include "abi.h"

ENTRY(sqrtl)
	fldt	8(%rsp)
	fsqrt
	ret
END_STD(sqrtl)
