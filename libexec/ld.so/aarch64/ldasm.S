/*	$OpenBSD: ldasm.S,v 1.2 2017/01/24 07:48:37 guenther Exp $ */

/*
 * Copyright (c) 2016 Dale Rahn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define DL_DATA_SIZE	(16 * 8)	/* XXX */
#include <machine/asm.h>
#include <sys/syscall.h>
#include <SYS.h>

ENTRY(_dl_start)
	mov	x29, sp
	// need to worry about alignment, I think not?
	mov	x19, sp
	sub	sp, sp, #8+8+DL_DATA_SIZE	// dl_data size
	add	x20, sp, #4			// dl_data
	mov	x21, lr				// save old lr

	mov	x0, x29				// original stack
	mov	x1, x20				// dl_data

	adrp	x2, :got:_DYNAMIC		/* &_DYNAMIC */
	add	x2, x2, #:lo12:_DYNAMIC

	bl	_dl_boot_bind

	add	x0, x29, #8			// argv
	ldr	x1, [x29]			// load argc
	add	x1, x0, x1, lsl #3		// envp = argv + argc * 8
	add	x1, x1, #8			//                        + 8
	ldr	x2, [x20, #7*8]			// loff from dl_data
	mov	x3, x20				// dl_data
	bl	_dl_boot

	mov	sp, x29				// move stack back
	mov	x29, xzr			// clear frame back pointer
	mov	lr, xzr

	adrp	x8, :got:_dl_dtors
	ldr	x2, [x8, :got_lo12:_dl_dtors]

	br	x0

ENTRY(_dl_bind_start)
	/*
	 * ip is pointer to got entry for this relocation
	 * lr is pointer to pltgot[2], which is entry -1 of got plt reloc.
	 * return address is on stack
	 */
	stp	x29, x30, [sp, #-160]!
	stp	 x0,  x1, [sp,#16]
	stp	 x2,  x3, [sp,#32]
	stp	 x4,  x5, [sp,#48]
	stp	 x6,  x7, [sp,#64]
	stp	 x8,  x9, [sp,#80]
	stp	x10, x11, [sp,#96]
	stp	x12, x13, [sp,#112]
	stp	x14, x15, [sp,#128]
	str	x18,      [sp,#144]

	mov	x1, x16		// reladdr


	ldp	 x0,  x1, [sp,#16]
	ldp	 x2,  x3, [sp,#32]
	ldp	 x4,  x5, [sp,#48]
	ldp	 x6,  x7, [sp,#64]
	ldp	 x8,  x9, [sp,#80]
	ldp	x10, x11, [sp,#96]
	ldp	x12, x13, [sp,#112]
	ldp	x14, x15, [sp,#128]
	ldr	x18,      [sp,#144]
	ldp	x29, x30, [sp], #-160
	br x16


#if 0
	stmdb	sp!, {r0-r4,sl,fp}

	sub	r1, ip, lr		/* r1 = 4 * (n + 1) */
	sub	r1, r1, #4		/* r1 = 4 * n */
	mov	r1, r1, lsr #2		/* r1 = n */

	ldr	r0, [lr, #-4]
	bl	_dl_bind
	mov	ip, r0
	ldmia	sp!, {r0-r4,sl,fp,lr}
	mov	pc, ip
#endif

	/* STUB */


/* ld.so SYSCALLS */

#define DL_SYSCALL(n) DL_SYSCALL2(n,n)
#define DL_SYSCALL2(n,c)					\
	.global		__CONCAT(_dl_,n)		;\
	.type		__CONCAT(_dl_,n)%function	;\
__CONCAT(_dl_,n):					;\
	SYSTRAP(c)					;\
	bcs	.L_cerr					;\
	ret

	.section	".text"
	.align		4
DL_SYSCALL(close)


	.global		_dl_exit
	.type		_dl_exit%function
_dl_exit:
	SYSTRAP(exit)
	1:
		b 1b

DL_SYSCALL(issetugid)
DL_SYSCALL(getthrid)
DL_SYSCALL2(_syscall,__syscall)
DL_SYSCALL(munmap)
DL_SYSCALL(mprotect)
DL_SYSCALL(open)
DL_SYSCALL(read)
DL_SYSCALL(write)
DL_SYSCALL(fstat)
DL_SYSCALL(readlink)
DL_SYSCALL(utrace)
DL_SYSCALL(getentropy)
DL_SYSCALL(sendsyslog)
DL_SYSCALL(pledge)
DL_SYSCALL2(getcwd,__getcwd)
DL_SYSCALL(sysctl)
DL_SYSCALL2(set_tcb,__set_tcb)
DL_SYSCALL(thrkill)

DL_SYSCALL(getdents)

	.global		_dl_sigprocmask
	.type		_dl_sigprocmask%function
_dl_sigprocmask:
#if 0
	teq	r1, #0
	moveq	r0, #1
	moveq	r1, #0
	ldrne	r1, [r1]
	SYSTRAP(sigprocmask)
	bcs	.L_cerr
	teq	r2, #0
	strne	r0, [r2]
	mov	r0, #0
	mov	pc, lr
#endif

.L_cerr:
	neg	w0, w0		/* r0 = -errno */
	ret

ENTRY(_rtld_tlsdesc)
	ldr	x0, [x0, #8]
	ret
