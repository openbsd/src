/*
 * Written by J.T. Conklin, Apr 4, 1995
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: fpgetmask.S,v 1.2 1996/08/19 08:12:22 tholo Exp $"
#endif

ENTRY(fpgetmask)
	subl $4,%esp
	fnstcw (%esp)
	movl (%esp),%eax
	notl %eax
	andl $63,%eax
	addl $4,%esp
	ret
