/*	$OpenBSD: uthread_machdep_asm.S,v 1.4 2002/08/25 22:10:44 brad Exp $	*/
/* David Leonard <d@csee.uq.edu.au>. Public domain. */

#include <machine/asm.h>
#include <machine/trap.h>
#include <machine/frame.h>

/*
 * Switch stacks.
 * On sparc this also means we switch register windows.
 */

#ifdef __sparcv9__
#define flushw		.word 0x81580000
#else
#define flushw		t T_FLUSHWIN
#endif

/*
 * void _thread_machdep_switch(long newstate[2], long savestate[2], int flags);
 */
ENTRY(_thread_machdep_switch)

	/* new window */
	save	%sp, -CC64FSZ, %sp

	/* flush all windows (except current one) into memory frames */
	flushw

	/* switch the stack pointer and return address */
	stx 	%fp, [%i1 + 0]
	stx	%i7, [%i1 + 8]
	ldx	[%i0 + 0], %fp
	ldx	[%i0 + 8], %i7

	/* return to saved window at new %fp */
	ret
	 restore

