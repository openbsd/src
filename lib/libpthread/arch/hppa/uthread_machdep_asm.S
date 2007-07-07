/*	$OpenBSD: uthread_machdep_asm.S,v 1.3 2007/07/07 15:46:56 kettenis Exp $	*/
/*	Michael Shalayeff <mickey@openbsd.org>. Public Domain. */

#include <machine/asm.h>
#define	_LOCORE
#include <machine/frame.h>

#define	FRAMESIZE	0x60

/*
 * void _thread_machdep_init(statep, base, len, entry)
 *	struct _machdep_state *statep;
 *	void *base;
 *	int len;
 *	void (*entry)(void);
 */
ENTRY(_thread_machdep_init,FRAMESIZE)
	ldo	7(arg1), arg1
	dep	r0, 31, 3, arg1
	ldo	HPPA_FRAME_SIZE(arg1), arg1
	ldo	FRAMESIZE(arg1), t1
	stw	t1, 0(arg0)
	stw	arg1, 4(arg0)

	bb,>=,n	arg3, 30, _thread$noplabel
	depi	0, 31, 2, arg3
	ldw	4(arg3), t1
	ldw	0(arg3), arg3
	stw	t1, 0x44(arg1)

_thread$noplabel
	bv	r0(rp)
	stw	arg3, 0(arg1)
EXIT(_thread_machdep_init)

/*
 * void _thread_machdep_switch(newstate, oldstate);
 *	struct _machdep_state *newstate, *oldstate;
 */
ENTRY(_thread_machdep_switch,0)
	copy	sp, t1
	ldo	FRAMESIZE(sp), sp
	stw	sp, 0(arg1)
	stw	t1, 4(arg1)
	stw	 r2, 0x00(t1)
	stw	 r3, 0x04(t1)
	stw	 r4, 0x08(t1)
	stw	 r5, 0x0c(t1)
	stw	 r6, 0x10(t1)
	stw	 r7, 0x14(t1)
	stw	 r8, 0x18(t1)
	stw	 r9, 0x1c(t1)
	stw	r10, 0x20(t1)
	stw	r11, 0x24(t1)
	stw	r12, 0x28(t1)
	stw	r13, 0x2c(t1)
	stw	r14, 0x30(t1)
	stw	r15, 0x34(t1)
	stw	r16, 0x38(t1)
	stw	r17, 0x3c(t1)
	stw	r18, 0x40(t1)
	stw	r19, 0x44(t1)

	ldw	0(arg0), sp
	ldw	4(arg0), t1
	ldw	0x00(t1), r2
	ldw	0x04(t1), r3
	ldw	0x08(t1), r4
	ldw	0x0c(t1), r5
	ldw	0x10(t1), r6
	ldw	0x14(t1), r7
	ldw	0x18(t1), r8
	ldw	0x1c(t1), r9
	ldw	0x20(t1), r10
	ldw	0x24(t1), r11
	ldw	0x28(t1), r12
	ldw	0x2c(t1), r13
	ldw	0x30(t1), r14
	ldw	0x34(t1), r15
	ldw	0x38(t1), r16
	ldw	0x3c(t1), r17
	ldw	0x40(t1), r18
	ldw	0x44(t1), r19
	bv	r0(rp)
	ldo	-FRAMESIZE(sp), sp
EXIT(_thread_machdep_switch)

/*
 * void _thread_machdep_save_float_state(struct _machdep_state* statep);
 */
ENTRY(_thread_machdep_save_float_state,0)
	ldo	8(arg0), arg0
	fstds,ma fr0 , 8(arg0)
	fstds,ma fr1 , 8(arg0)
	fstds,ma fr2 , 8(arg0)
	fstds,ma fr3 , 8(arg0)
	fstds,ma fr4 , 8(arg0)
	fstds,ma fr5 , 8(arg0)
	fstds,ma fr6 , 8(arg0)
	fstds,ma fr7 , 8(arg0)
	fstds,ma fr8 , 8(arg0)
	fstds,ma fr9 , 8(arg0)
	fstds,ma fr10, 8(arg0)
	fstds,ma fr11, 8(arg0)
	fstds,ma fr12, 8(arg0)
	fstds,ma fr13, 8(arg0)
	fstds,ma fr14, 8(arg0)
	fstds,ma fr15, 8(arg0)
	fstds,ma fr16, 8(arg0)
	fstds,ma fr17, 8(arg0)
	fstds,ma fr18, 8(arg0)
	fstds,ma fr19, 8(arg0)
	fstds,ma fr20, 8(arg0)
	fstds,ma fr21, 8(arg0)
	fstds,ma fr22, 8(arg0)
	fstds,ma fr23, 8(arg0)
	fstds,ma fr24, 8(arg0)
	fstds,ma fr25, 8(arg0)
	fstds,ma fr26, 8(arg0)
	fstds,ma fr27, 8(arg0)
	fstds,ma fr28, 8(arg0)
	fstds,ma fr29, 8(arg0)
	fstds,ma fr30, 8(arg0)
	bv	r0(rp)
	fstd,ma	fr31, 8(arg0)
EXIT(_thread_machdep_save_float_state)

/*
 * void _thread_machdep_restore_float_state(struct _machdep_state* statep);
 */
ENTRY(_thread_machdep_restore_float_state,0)
	ldo	8*32(arg0), arg0
	fldds,ma -8(arg0), fr31
	fldds,ma -8(arg0), fr30
	fldds,ma -8(arg0), fr29
	fldds,ma -8(arg0), fr28
	fldds,ma -8(arg0), fr27
	fldds,ma -8(arg0), fr26
	fldds,ma -8(arg0), fr25
	fldds,ma -8(arg0), fr24
	fldds,ma -8(arg0), fr23
	fldds,ma -8(arg0), fr22
	fldds,ma -8(arg0), fr21
	fldds,ma -8(arg0), fr20
	fldds,ma -8(arg0), fr19
	fldds,ma -8(arg0), fr18
	fldds,ma -8(arg0), fr17
	fldds,ma -8(arg0), fr16
	fldds,ma -8(arg0), fr15
	fldds,ma -8(arg0), fr14
	fldds,ma -8(arg0), fr13
	fldds,ma -8(arg0), fr12
	fldds,ma -8(arg0), fr11
	fldds,ma -8(arg0), fr10
	fldds,ma -8(arg0), fr9
	fldds,ma -8(arg0), fr8
	fldds,ma -8(arg0), fr7
	fldds,ma -8(arg0), fr6
	fldds,ma -8(arg0), fr5
	fldds,ma -8(arg0), fr4
	fldds,ma -8(arg0), fr3
	fldds,ma -8(arg0), fr2
	fldds,ma -8(arg0), fr1
	bv	%r0(rp)
	fldds,ma -8(arg0), fr0
EXIT(_thread_machdep_restore_float_state)

	.end
