/*
 * Written by J.T. Conklin, Apr 4, 1995
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: fpgetround.S,v 1.2 1996/08/19 08:12:23 tholo Exp $"
#endif

ENTRY(fpgetround)
	subl $4,%esp
	fnstcw (%esp)
	movl (%esp),%eax
	rorl $10,%eax
	andl $3,%eax
	addl $4,%esp
	ret
