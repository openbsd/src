/*	$OpenBSD: fabs.S,v 1.1 2004/01/28 01:44:45 mickey Exp $	*/
/*	$NetBSD: fabs.S,v 1.1 2001/06/19 00:25:02 fvdl Exp $	*/

#include <machine/asm.h>

/*
 * Ok, this sucks. Is there really no way to push an xmm register onto
 * the FP stack directly?
 */

ENTRY(fabs)
	movsd	%xmm0, -8(%rsp)
	fldl	-8(%rsp)
	fabs
	fstp	-8(%rsp)
	movsd	-8(%rsp),%xmm0
	ret
