/*	$OpenBSD: ldasm.S,v 1.6 2006/05/03 16:10:52 drahn Exp $ */

/*
 * Copyright (c) 2004 Dale Rahn
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

#define DL_DATA_SIZE	(16 * 4)	/* XXX */
#include <machine/asm.h>
#include <sys/syscall.h>
#include <SYS.h>

ENTRY(_dl_start)
	mov	fp, sp
	mov	r5, sp
	mov	lr, r6				@ save lr
	sub	sp, sp, #4+4+DL_DATA_SIZE
	add	r7, sp, #4			@ dl_data

	mov	r0, fp				@ original stack
	mov	r1, r7				@ dl_data

	bl	_dl_boot_bind

	add	r0, r5, #4			@ argv
	ldr	r1, [r5, #0x0]			@ envp
	add	r1, r1, #2
	add	r1, fp, r1, lsl #2
	ldr	r2, [r7, #7*4]			@ loff from dl_data
	mov	r3, r7				@ dl_data
	bl	_dl_boot

	mov	sp, fp
	mov	fp, #0
	mov	lr, r6

	mov	pc, r0


ENTRY(_dl_bind_start)
	/*
	 * ip is pointer to got entry for this relocation
	 * lr is pointer to pltgot[2], which is entry -1 of got plt reloc.
	 * return address is on stack
	 */
	stmdb	sp!, {r0-r4,sl,fp}

	sub	r1, ip, lr		/* r1 = 4 * (n + 1) */
	sub	r1, r1, #4		/* r1 = 4 * n */
	mov	r1, r1, lsr #2		/* r1 = n */

	ldr	r0, [lr, #-4]
	bl	_dl_bind
	mov	ip, r0
	ldmia	sp!, {r0-r4,sl,fp,lr}
	mov	pc, ip

	/* STUB */


/* ld.so SYSCALLS */

#define DL_SYSCALL(n) DL_SYSCALL2(n,n)
#define DL_SYSCALL2(n,c)					\
	.global		__CONCAT(_dl_,n)		;\
	.type		__CONCAT(_dl_,n)%function	;\
__CONCAT(_dl_,n):					;\
	SYSTRAP(c)					;\
	bcs	.L_cerr					;\
	mov	pc, lr

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
DL_SYSCALL2(_syscall,__syscall)
DL_SYSCALL(munmap)
DL_SYSCALL(mprotect)
DL_SYSCALL(open)
DL_SYSCALL(read)
DL_SYSCALL(write)
DL_SYSCALL(stat)
DL_SYSCALL(fstat)
DL_SYSCALL(fcntl)
DL_SYSCALL(gettimeofday)
DL_SYSCALL2(sysctl,__sysctl)

DL_SYSCALL(getdirentries)

	.global		_dl_sigprocmask
	.type		_dl_sigprocmask%function
_dl_sigprocmask:
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

.L_cerr:
	mov	r0, #0
	sub	r0, r0, #1
	mov	pc, lr
