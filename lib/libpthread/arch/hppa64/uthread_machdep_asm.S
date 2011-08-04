/*	$OpenBSD: uthread_machdep_asm.S,v 1.1 2011/08/04 14:23:36 kettenis Exp $	*/
/*	Michael Shalayeff <mickey@openbsd.org>. Public Domain. */

#include <machine/asm.h>
#define	_LOCORE
#include <machine/frame.h>

/*
 * void _thread_machdep_init(statep, base, len, entry)
 *	struct _machdep_state *statep;
 *	void *base;
 *	int len;
 *	void (*entry)(void);
 */
ENTRY(_thread_machdep_init,0)
	bv	%r0(%rp)
	 nop
EXIT(_thread_machdep_init)

/*
 * void _thread_machdep_switch(newstate, oldstate);
 *	struct _machdep_state *newstate, *oldstate;
 */
ENTRY(_thread_machdep_switch,0)
	bv	%r0(%rp)
	nop
EXIT(_thread_machdep_switch)

/*
 * void _thread_machdep_save_float_state(struct _machdep_state* statep);
 */
ENTRY(_thread_machdep_save_float_state,0)
	ldo	8(%arg0), %arg0
	fstd,ma %fr0,  8(%arg0)
	fstd,ma %fr1,  8(%arg0)
	fstd,ma %fr2,  8(%arg0)
	fstd,ma %fr3,  8(%arg0)
	fstd,ma %fr4,  8(%arg0)
	fstd,ma %fr5,  8(%arg0)
	fstd,ma %fr6,  8(%arg0)
	fstd,ma %fr7,  8(%arg0)
	fstd,ma %fr8,  8(%arg0)
	fstd,ma %fr9,  8(%arg0)
	fstd,ma %fr10, 8(%arg0)
	fstd,ma %fr11, 8(%arg0)
	fstd,ma %fr12, 8(%arg0)
	fstd,ma %fr13, 8(%arg0)
	fstd,ma %fr14, 8(%arg0)
	fstd,ma %fr15, 8(%arg0)
	fstd,ma %fr16, 8(%arg0)
	fstd,ma %fr17, 8(%arg0)
	fstd,ma %fr18, 8(%arg0)
	fstd,ma %fr19, 8(%arg0)
	fstd,ma %fr20, 8(%arg0)
	fstd,ma %fr21, 8(%arg0)
	fstd,ma %fr22, 8(%arg0)
	fstd,ma %fr23, 8(%arg0)
	fstd,ma %fr24, 8(%arg0)
	fstd,ma %fr25, 8(%arg0)
	fstd,ma %fr26, 8(%arg0)
	fstd,ma %fr27, 8(%arg0)
	fstd,ma %fr28, 8(%arg0)
	fstd,ma %fr29, 8(%arg0)
	fstd,ma %fr30, 8(%arg0)
	bv	%r0(%rp)
	fstd,ma	%fr31, 8(%arg0)
EXIT(_thread_machdep_save_float_state)

/*
 * void _thread_machdep_restore_float_state(struct _machdep_state* statep);
 */
ENTRY(_thread_machdep_restore_float_state,0)
	ldo	8*32(%arg0), %arg0
	fldd,ma -8(%arg0), %fr31
	fldd,ma -8(%arg0), %fr30
	fldd,ma -8(%arg0), %fr29
	fldd,ma -8(%arg0), %fr28
	fldd,ma -8(%arg0), %fr27
	fldd,ma -8(%arg0), %fr26
	fldd,ma -8(%arg0), %fr25
	fldd,ma -8(%arg0), %fr24
	fldd,ma -8(%arg0), %fr23
	fldd,ma -8(%arg0), %fr22
	fldd,ma -8(%arg0), %fr21
	fldd,ma -8(%arg0), %fr20
	fldd,ma -8(%arg0), %fr19
	fldd,ma -8(%arg0), %fr18
	fldd,ma -8(%arg0), %fr17
	fldd,ma -8(%arg0), %fr16
	fldd,ma -8(%arg0), %fr15
	fldd,ma -8(%arg0), %fr14
	fldd,ma -8(%arg0), %fr13
	fldd,ma -8(%arg0), %fr12
	fldd,ma -8(%arg0), %fr11
	fldd,ma -8(%arg0), %fr10
	fldd,ma -8(%arg0), %fr9
	fldd,ma -8(%arg0), %fr8
	fldd,ma -8(%arg0), %fr7
	fldd,ma -8(%arg0), %fr6
	fldd,ma -8(%arg0), %fr5
	fldd,ma -8(%arg0), %fr4
	fldd,ma -8(%arg0), %fr3
	fldd,ma -8(%arg0), %fr2
	fldd,ma -8(%arg0), %fr1
	bv	%r0(%rp)
	fldd,ma -8(%arg0), %fr0
EXIT(_thread_machdep_restore_float_state)

	.end
