/*	$OpenBSD: ldasm.S,v 1.6 2010/02/01 21:36:00 kettenis Exp $	*/

/*
 * Copyright (c) 2004 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/syscall.h>
#include <machine/asm.h>
#define	_LOCORE
#include <machine/frame.h>
#include <machine/vmparam.h>
#undef	_LOCORE

ENTRY(_dl_start,32)
	copy	r3, r1
	copy	sp, r3
	stwm	r1, HPPA_FRAME_SIZE+16*4(sp)

	stw	%arg0, HPPA_FRAME_ARG(0)(r3)	/* ps_strings */

#define	ADDR(s,r) \
	bl	4, t1							!\
	depi	0, 31, 2, t1	/* kill pl bits */			!\
	b	s		/* cold brunch -- never done */		!\
	ldw	0(t1), t2	/* cat(w,w1,w2{10},w2{0..9}) << 2 */	!\
	extru	t2, 28, 10, t3	/* w2{0..9} */				!\
	extru	t2, 26, 16, r	/* w1 */				!\
	dep	t3, 31, 11, r						!\
	extru,=	t2, 31, 1, r0	/* w */					!\
	depi	1, 15, 1, r						!\
	extru,=	t2, 29, 1, r0	/* w2{10} */				!\
	depi	1, 21, 1, r						!\
	sh2add	r, t1, r	/* plus the base */			!\
	addi	8, r, r		/* bl target is -8 */

	ADDR(_GLOBAL_OFFSET_TABLE_, r19)
	ADDR(_DYNAMIC, arg2)
	stw	arg2, HPPA_FRAME_ARG(1)(r3)

	/* make sure to get a fault until it's set proper */
	ldi	-1, %dp

	ldw	0(arg0), arg0
	ldo	4(r3), arg1			/* dl_data */
	bl	_dl_boot_bind, rp
	ldo	-4(arg0), arg0

	ldw	HPPA_FRAME_ARG(1)(r3), arg1	/* &_DYNAMIC */
	ldw	HPPA_FRAME_ARG(0)(r3), arg3	/* ps_strings */
	ldw	0(r19), arg2
	sub	arg1, arg2, arg2		/* loff */

	ldw	0(arg3), arg0			/* argv */
	ldw	8(arg3), arg1			/* envp */

	bl	_dl_boot, rp
	ldo	4(r3), arg3			/* dl_data */

	ldw	HPPA_FRAME_ARG(0)(r3), arg0	/* ps_strings */
	/* ??? cleanup, arg1 */
	/* ??? obj, arg2 */

	ldo	HPPA_FRAME_SIZE(r3), sp
	copy	r0, rp
	bv	r0(ret0)
	ldwm	-HPPA_FRAME_SIZE(sp), r3
EXIT(_dl_start)

/*
 * void _hppa_dl_dtors(void);
 */
ENTRY(_hppa_dl_dtors,0)
	ADDR(_GLOBAL_OFFSET_TABLE_, r19)
	b	_dl_dtors
	nop
EXIT(_hppa_dl_dtors)

LEAF_ENTRY(_hppa_dl_set_dp)
	bv	r0(rp)
	copy	arg0, r27
EXIT(_hppa_dl_set_dp)

/*
 * void hppa_call(void (*arg)(void), void *r19, Elf_Addr func);
 */
ENTRY(hppa_call,0)
	copy	r3, r1
	copy	sp, r3
	stwm	r1, HPPA_FRAME_SIZE(sp)
	stw	r19, HPPA_FRAME_SL(r3)
	stw	rp, HPPA_FRAME_CRP(r3)
	copy	arg1, r19
	.call
	blr	r0, rp
	bv,n	(arg2)
	nop
	ldw	HPPA_FRAME_CRP(r3), rp
	ldw	HPPA_FRAME_SL(r3), r19
	ldo	HPPA_FRAME_SIZE(r3), sp
	bv	r0(rp)
	ldwm	-HPPA_FRAME_SIZE(sp), r3
EXIT(hppa_call)

/*
 * This is a magic branch instruction that is used by GCC's
 * __canonicalize_funcptr_for_compare() function to fixup relocations
 * in order to do function pointer comparisons.
 */
	bl	_dl_bind, rp

ENTRY(_dl_bind_start,32)
	copy	r3, r1
	copy	sp, r3
	stwm	r1, HPPA_FRAME_SIZE(sp)

	stw	rp, HPPA_FRAME_CRP(r3)
	stw	arg0, HPPA_FRAME_ARG(0)(r3)
	stw	arg1, HPPA_FRAME_ARG(1)(r3)
	stw	arg2, HPPA_FRAME_ARG(2)(r3)
	stw	arg3, HPPA_FRAME_ARG(3)(r3)
	stw	t1, 4(r3)
	stw	ret0, 8(r3)
	stw	ret1, 12(r3)

	ldw	12(r20), arg0
	copy	r19, arg1

	bl	_dl_bind, rp
	copy	r21, r19

	/* load &func and sl from plt entry returned */
	ldw	0(ret0), r21
	ldw	4(ret0), r19

	ldw	HPPA_FRAME_ARG(0)(r3), arg0
	ldw	HPPA_FRAME_ARG(1)(r3), arg1
	ldw	HPPA_FRAME_ARG(2)(r3), arg2
	ldw	HPPA_FRAME_ARG(3)(r3), arg3
	ldw	4(r3), t1
	ldw	8(r3), ret0
	ldw	12(r3), ret1

	ldw	HPPA_FRAME_CRP(r3), rp
	ldo	HPPA_FRAME_SIZE(r3), sp
	bv	r0(r21)
	ldwm	-HPPA_FRAME_SIZE(sp), r3
EXIT(_dl_bind_start)

#define	SYSCALL(x)				!\
	stw	rp, HPPA_FRAME_ERP(sr0,sp)	!\
	ldil	L%SYSCALLGATE, r1		!\
	ble	4(sr7, r1)			!\
	ldi	__CONCAT(SYS_,x), t1		!\
	comb,<>	r0, t1, _dl_sysexit		!\
	ldw	HPPA_FRAME_ERP(sr0,sp), rp

_dl_sysexit
	bv	r0(rp)
	sub	r0, ret0, ret0

ENTRY(_dl_close,0)
	SYSCALL(close)
	bv	r0(rp)
	nop
EXIT(_dl_close)

ENTRY(_dl_exit,0)
	SYSCALL(exit)
	bv	r0(rp)
	nop
EXIT(_dl_exit)

ENTRY(_dl_issetugid,0)
	SYSCALL(issetugid)
	bv	r0(rp)
	nop
EXIT(_dl_issetugid)

ENTRY(_dl__syscall,0)
	SYSCALL(__syscall)
	bv	r0(rp)
	nop
EXIT(_dl__syscall)

ENTRY(_dl_munmap,0)
	SYSCALL(munmap)
	bv	r0(rp)
	nop
EXIT(_dl_munmap)

ENTRY(_dl_mprotect,0)
	SYSCALL(mprotect)
	bv	r0(rp)
	nop
EXIT(_dl_mprotect)

ENTRY(_dl_open,0)
	SYSCALL(open)
	bv	r0(rp)
	nop
EXIT(_dl_open)

ENTRY(_dl_read,0)
	SYSCALL(read)
	bv	r0(rp)
	nop
EXIT(_dl_read)

ENTRY(_dl_write,0)
	SYSCALL(write)
	bv	r0(rp)
	nop
EXIT(_dl_write)

ENTRY(_dl_stat,0)
	SYSCALL(stat)
	bv	r0(rp)
	nop
EXIT(_dl_stat)

ENTRY(_dl_fstat,0)
	SYSCALL(fstat)
	bv	r0(rp)
	nop
EXIT(_dl_fstat)

ENTRY(_dl_fcntl,0)
	SYSCALL(fcntl)
	bv	r0(rp)
	nop
EXIT(_dl_fcntl)

ENTRY(_dl_sysctl,0)
	SYSCALL(__sysctl)
	bv	r0(rp)
	nop
EXIT(_dl_issetugid)

ENTRY(_dl_getdirentries,0)
	SYSCALL(getdirentries)
	bv	r0(rp)
	nop
EXIT(_dl_getdirentries)

ENTRY(_dl_gettimeofday,0)
	SYSCALL(gettimeofday)
	bv	r0(rp)
	nop
EXIT(_dl_gettimeofday)

ENTRY(_dl_sigprocmask,0)
	stw	arg2, HPPA_FRAME_ARG(2)(sp)

	comb,<>,n r0, arg1, _dl_sigprocmask$nblock

	b	_dl_sigprocmask$call
	ldi	1, arg0

_dl_sigprocmask$nblock
	ldw	0(arg1), arg1
	stw	arg1, HPPA_FRAME_ARG(1)(sp)

_dl_sigprocmask$call
	SYSCALL(sigprocmask)

	ldw	HPPA_FRAME_ARG(2)(sp), arg2
	add,=	r0, arg2, r0
	stw	ret0, 0(arg2)
	bv	r0(rp)
	copy	r0, ret0
EXIT(_dl_sigprocmask)

	.end
