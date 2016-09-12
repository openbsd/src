/*	$OpenBSD: s_llrintf.S,v 1.2 2016/09/12 19:47:01 guenther Exp $	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <machine/asm.h>
#include "abi.h"

ENTRY(llrintf)
	cvtss2si %xmm0, %rax
	ret
END_STD(llrintf)
