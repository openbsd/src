/* $OpenBSD: flt_rounds.S,v 1.4 2012/08/22 17:19:34 pascal Exp $ */
/*
 * Written by J.T. Conklin, Apr 4, 1995
 * Public domain.
 */

#include <machine/asm.h>

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
#ifdef __PIC__
	PIC_PROLOGUE
	leal PIC_GOTOFF(_map),%ecx
	PIC_EPILOGUE
	movb (%ecx,%eax,1),%al
#else
	movb _map(,%eax,1),%al
#endif
	addl $4,%esp
	ret
