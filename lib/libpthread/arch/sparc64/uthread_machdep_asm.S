/*	$OpenBSD: uthread_machdep_asm.S,v 1.5 2003/01/24 21:05:45 jason Exp $	*/
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

#define	FPRS_ENA	0x4		/* fpu enabled */
#define	FPRS_DU		0x2		/* lower unit dirty */
#define	FPRS_DL		0x1		/* upper unit dirty */

ENTRY(_thread_machdep_fpsave)
	rd	%fprs, %o1
	stx	%o1, [%o0]
	stx	%fsr, [%o0 + 8]
	add	%o0, 16, %o0

	std	%f0, [%o0 + 0 * 8]		/* store registers */
	std	%f2, [%o0 + 1 * 8]
	std	%f4, [%o0 + 2 * 8]
	std	%f6, [%o0 + 3 * 8]
	std	%f8, [%o0 + 4 * 8]
	std	%f10, [%o0 + 5 * 8]
	std	%f12, [%o0 + 6 * 8]
	std	%f14, [%o0 + 7 * 8]
	std	%f16, [%o0 + 8 * 8]
	std	%f18, [%o0 + 9 * 8]
	std	%f20, [%o0 + 10 * 8]
	std	%f22, [%o0 + 11 * 8]
	std	%f24, [%o0 + 12 * 8]
	std	%f26, [%o0 + 13 * 8]
	std	%f28, [%o0 + 14 * 8]
	std	%f30, [%o0 + 15 * 8]

	std	%f32, [%o0 + 16 * 8]		/* store registers */
	std	%f34, [%o0 + 17 * 8]
	std	%f36, [%o0 + 18 * 8]
	std	%f38, [%o0 + 19 * 8]
	std	%f40, [%o0 + 20 * 8]
	std	%f42, [%o0 + 21 * 8]
	std	%f44, [%o0 + 22 * 8]
	std	%f46, [%o0 + 23 * 8]
	std	%f48, [%o0 + 24 * 8]
	std	%f50, [%o0 + 25 * 8]
	std	%f52, [%o0 + 26 * 8]
	std	%f54, [%o0 + 27 * 8]
	std	%f56, [%o0 + 28 * 8]
	std	%f58, [%o0 + 29 * 8]
	std	%f60, [%o0 + 30 * 8]
	std	%f62, [%o0 + 31 * 8]

	retl
	 nop

ENTRY(_thread_machdep_fprestore)
	ldx	[%o0], %o1		/* o1 = fprs */
	wr	%o1, 0, %fprs
	ldx	[%o0 + 8], %fsr
	add	%o0, 16, %o0

	ldd	[%o0 + 0 * 8], %f0
	ldd	[%o0 + 1 * 8], %f2
	ldd	[%o0 + 2 * 8], %f4
	ldd	[%o0 + 3 * 8], %f6
	ldd	[%o0 + 4 * 8], %f8
	ldd	[%o0 + 5 * 8], %f10
	ldd	[%o0 + 6 * 8], %f12
	ldd	[%o0 + 7 * 8], %f14
	ldd	[%o0 + 8 * 8], %f16
	ldd	[%o0 + 9 * 8], %f18
	ldd	[%o0 + 10 * 8], %f20
	ldd	[%o0 + 11 * 8], %f22
	ldd	[%o0 + 12 * 8], %f24
	ldd	[%o0 + 13 * 8], %f26
	ldd	[%o0 + 14 * 8], %f28
	ldd	[%o0 + 15 * 8], %f30

	ldd	[%o0 + 16 * 8], %f32
	ldd	[%o0 + 17 * 8], %f34
	ldd	[%o0 + 18 * 8], %f36
	ldd	[%o0 + 19 * 8], %f38
	ldd	[%o0 + 20 * 8], %f40
	ldd	[%o0 + 21 * 8], %f42
	ldd	[%o0 + 22 * 8], %f44
	ldd	[%o0 + 23 * 8], %f46
	ldd	[%o0 + 24 * 8], %f48
	ldd	[%o0 + 25 * 8], %f50
	ldd	[%o0 + 26 * 8], %f52
	ldd	[%o0 + 27 * 8], %f54
	ldd	[%o0 + 28 * 8], %f56
	ldd	[%o0 + 29 * 8], %f58
	ldd	[%o0 + 30 * 8], %f60
	ldd	[%o0 + 31 * 8], %f62

	retl
	 nop
