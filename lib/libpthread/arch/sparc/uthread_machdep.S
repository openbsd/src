/*	$OpenBSD: uthread_machdep.S,v 1.1 2000/01/06 07:04:36 d Exp $	*/

#include <machine/asm.h>
#include <machine/trap.h>

ENTRY(_thread_machdep_create)
	st	%o2, [%o0]	/* store the new stack pointer */
	sub	%o1, 8, %o1	/* the return adress is o7 + 8 ! */
	st	%o1, [%o0+4]	/* store the new pc */

	retl
	nop

ENTRY(_thread_machdep_setjmp)
	t	ST_FLUSHWIN	/* flush register windows */
	st	%sp, [%o0]	/* store sp */
	st	%o7, [%o0+4]	/* store pc */
	retl
	clr %o0			/* return 0 */

ENTRY(_thread_machdep_longjmp)
	t	ST_FLUSHWIN	/* flush register windows */
	sub	%sp, 64, %sp	/* foo? */

	ld	[%o0+4], %o7	/* restore pc */
	ld	[%o0], %fp	/* restore fp, the restore ins below will
				 * cause it to become sp */
	/* i?i:1 - Do we need to care? it's always called with 1 */
	tst	%o1
	be,a	0f
	mov 1, %o1
0:
	retl
	restore %o1, 0, %o0

