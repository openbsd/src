/*	$OpenBSD: uthread_machdep_asm.S,v 1.6 2003/01/26 20:24:36 jason Exp $	*/
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

ENTRY(_thread_machdep_fpsave)
	/*
	 * If %psr were readable, exitting could be faster.  The EF
	 * bit is only set if the FPU is enabled, and we only need
	 * to save fpu state if it is enabled.  But, RDPSR is a
	 * priviledged instruction.
	 */

	/* save registers */
	std	%f0, [%o1 + 0 * 8]
	std	%f2, [%o1 + 1 * 8]
	std	%f4, [%o1 + 2 * 8]
	std	%f6, [%o1 + 3 * 8]
	std	%f8, [%o1 + 4 * 8]
	std	%f10, [%o1 + 5 * 8]
	std	%f12, [%o1 + 6 * 8]
	std	%f14, [%o1 + 7 * 8]
	std	%f16, [%o1 + 8 * 8]
	std	%f18, [%o1 + 9 * 8]
	std	%f20, [%o1 + 10 * 8]
	std	%f22, [%o1 + 11 * 8]
	std	%f24, [%o1 + 12 * 8]
	std	%f26, [%o1 + 13 * 8]
	std	%f28, [%o1 + 14 * 8]
	std	%f30, [%o1 + 15 * 8]
	retl
	 nop

ENTRY(_thread_machdep_fprestore)
	ldd	[%o1 + 0 * 8], %f0
	ldd	[%o1 + 1 * 8], %f2
	ldd	[%o1 + 2 * 8], %f4
	ldd	[%o1 + 3 * 8], %f6
	ldd	[%o1 + 4 * 8], %f8
	ldd	[%o1 + 5 * 8], %f10
	ldd	[%o1 + 6 * 8], %f12
	ldd	[%o1 + 7 * 8], %f14
	ldd	[%o1 + 8 * 8], %f16
	ldd	[%o1 + 9 * 8], %f18
	ldd	[%o1 + 10 * 8], %f20
	ldd	[%o1 + 11 * 8], %f22
	ldd	[%o1 + 12 * 8], %f24
	ldd	[%o1 + 13 * 8], %f26
	ldd	[%o1 + 14 * 8], %f28
	ldd	[%o1 + 15 * 8], %f30
	retl
	 nop
