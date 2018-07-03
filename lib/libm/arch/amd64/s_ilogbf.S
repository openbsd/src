/*	$OpenBSD: s_ilogbf.S,v 1.5 2018/07/03 22:43:34 mortimer Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>
#include "abi.h"

ENTRY(ilogbf)
	RETGUARD_SETUP(ilogbf, r11)
	movss	%xmm0,-4(%rsp)
	flds	-4(%rsp)
	fxtract
	fstp	%st
	fistpl	-4(%rsp)
	movl	-4(%rsp),%eax
	RETGUARD_CHECK(ilogbf, r11)
	ret
END_STD(ilogbf)
