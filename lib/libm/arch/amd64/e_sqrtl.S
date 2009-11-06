/*	$OpenBSD: e_sqrtl.S,v 1.1 2009/11/06 23:30:21 kettenis Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(sqrtl)
	fldt	8(%rsp)
	fsqrt
	ret
