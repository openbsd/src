/*	$OpenBSD: flt_rounds.S,v 1.1 2004/01/28 01:44:45 mickey Exp $	*/
/*	$NetBSD: flt_rounds.S,v 1.2 2002/06/06 23:04:35 fvdl Exp $	*/

/*
 * Written by J.T. Conklin, Apr 4, 1995
 * Public domain.
 */

#include <machine/asm.h>

	.text
	_ALIGN_TEXT
_map:
	.byte 1		/* round to nearest */
	.byte 3		/* round to negative infinity */
	.byte 2		/* round to positive infinity */
	.byte 0		/* round to zero */

ENTRY(__flt_rounds)
	fnstcw -4(%rsp)
	movl -4(%rsp),%eax
	shrl $10,%eax
	andl $3,%eax
#ifdef PIC
	leaq PIC_GOT(_map),%rcx
	movb (%rcx,%rax,1),%al
#else
	movb _map(,%rax,1),%al
#endif
	ret
