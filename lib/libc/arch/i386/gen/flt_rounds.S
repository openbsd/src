/*
 * Written by J.T. Conklin, Apr 4, 1995
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: flt_rounds.S,v 1.2 1996/08/19 08:12:20 tholo Exp $"
#endif

	.text
	.align 2
_map:
	.byte 1		/* round to nearest */
	.byte 3		/* round to negative infinity */
	.byte 2		/* round to positive infinity */
	.byte 0		/* round to zero */

ENTRY(__flt_rounds)
	subl $4,%esp
	fnstcw (%esp)
	movl (%esp),%eax
	shrl $10,%eax
	andl $3,%eax
#ifdef PIC
	PIC_PROLOGUE
	leal PIC_GOTOFF(_map),%ecx
	PIC_EPILOGUE
	movb (%ecx,%eax,1),%al
#else
	movb _map(,%eax,1),%al
#endif
	addl $4,%esp
	ret
