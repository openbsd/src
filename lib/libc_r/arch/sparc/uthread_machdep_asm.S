/*	$OpenBSD: uthread_machdep_asm.S,v 1.5 2001/12/18 06:24:13 fgsch Exp $	*/
/* David Leonard <d@csee.uq.edu.au>. Public domain. */

#include <machine/asm.h>
#include <machine/trap.h>

/*
 * Switch stacks.
 * On sparc this also means we switch register windows.
 */

#ifdef __sparcv9__
#define flushw		.word 0x81580000
#else
#define flushw		t ST_FLUSHWIN
#endif

#define SA(x)	(((x)+7)&(~0x7))
#define MINFRAME ((16+1+6)*4)	

/*
 * void _thread_machdep_switch(newstate, oldstate);
 *	struct _machdep_state *newstate, *oldstate;
 */
ENTRY(_thread_machdep_switch)

	/* new window */
	save	%sp, -SA(MINFRAME), %sp

	/* flush all windows (except current one) into memory frames */
	flushw

	/* switch the stack pointer and return address */
	st 	%fp, [%i1 + 0]
	st	%i7, [%i1 + 4]
	ld	[%i0 + 0], %fp
	ld	[%i0 + 4], %i7

	/* return to saved window at new %fp */
	ret
	restore

