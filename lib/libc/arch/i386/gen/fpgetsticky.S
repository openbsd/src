/*
 * Written by J.T. Conklin, Apr 4, 1995
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: fpgetsticky.S,v 1.2 1996/08/19 08:12:24 tholo Exp $"
#endif

ENTRY(fpgetsticky)
	subl $4,%esp
	fnstsw (%esp)
	movl (%esp),%eax
	andl $63,%eax
	addl $4,%esp
	ret
