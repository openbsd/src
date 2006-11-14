/*	$OpenBSD: ldasm.S,v 1.7 2006/11/14 19:47:50 drahn Exp $ */

/*
 * Copyright (c) 2006 Dale Rahn
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
	mov	r15, r12			// save for later 
	sts	pr, r11
	mov	r15, r4			// boot_bind(sp, dl_data) (sp)
	mov.l	.L_datasize, r0
	sub	r0, r15
	mov	r15, r5
	mov	r5, r13
	// not trusting register to store the data, push it on the stack.
	// callee/caller save questions

	mov	r15, r14

	bsr	1f
	 nop
1:
.L_offbase:
	sts	pr, r0
	mov.l	.L_dynamic, r6
	add	r0, r6
	mov	 r14, r15
	mov	r15, r14
	mov.l	.L_boot_bind, r0
	bsrf	r0
	 nop
.L_call_boot_bind:
	mov	r12, r4
	add	#4, r4
	mov.l	@r12, r5		//loads argc
	add	#2, r5
	shll2	r5
	add	r12, r5			// calc argv 

	mov	r13, r7
	mov	r7, r6
	mov.l	.L_loff, r0
	add	r0, r6
	mov.l	@r6, r6

	mov.l	.L_boot, r0
	bsrf	r0
	 nop
.L_call_boot:

	mov	r12, r15
	lds	r11, pr
	mov.l	@r15, r4
	mov	r15, r5
	add	#4, r5

	mov	r4, r6
	add	#1, r6
	shll2	r6
	add	r5, r6			// calc envp
	jmp	@r0
	 nop

	.align 2
.L_boot_bind:
	.long _dl_boot_bind-.L_call_boot_bind
.L_boot:
	.long _dl_boot-.L_call_boot
.L_datasize:
	.long 4+4+DL_DATA_SIZE
.L_dynamic:
	.long _DYNAMIC-.L_offbase
.L_loff:
	.long 7*4
	.size _dl_start, .-dl_start


/*
 * r0 - obj
 * r1 - reloff
 */

ENTRY(_dl_bind_start)
	mov.l	r2, @-r15
	mov.l	r3, @-r15
	mov.l	r4, @-r15
	mov.l	r5, @-r15
	mov.l	r6, @-r15
	mov.l	r7, @-r15
	sts.l	pr, @-r15
	sts.l	macl, @-r15
	sts.l	mach, @-r15

	mov	r0, r4	 /* move obj to 'C' arg */
	mov.l	.L_dl_bind, r0
	bsrf r0
	 mov r1, r5	 /* move reloff to 'C' arg */
.L_call_dl_bind:

	lds.l	@r15+, mach
	lds.l	@r15+, macl
	lds.l	@r15+, pr
	mov.l	@r15+, r7
	mov.l	@r15+, r6
	mov.l	@r15+, r5
	mov.l	@r15+, r4
	mov.l	@r15+, r3
	jmp	@r0		/* jump to specified address */
	 mov.l	@r15+, r2

	.align 2
.L_dl_bind:
	.long	_dl_bind-.L_call_dl_bind
	.size _dl_bind_start, .-dl_bind_start


	/* STUB */


/* ld.so SYSCALLS */

#define DL_SYSCALL(n) DL_SYSCALL2(n,n)
#define DL_SYSCALL2(n,c)				\
	.global		__CONCAT(_dl_,n)		;\
	.type		__CONCAT(_dl_,n)%function	;\
__CONCAT(_dl_,n):					;\
	SYSTRAP(c)					;\
	bf	.L_cerr					;\
	 nop						;\
	rts						;\
	 nop

#define DL_SYSCALL2_NOERR(n,c)				\
	.global		__CONCAT(_dl_,n)		;\
	.type		__CONCAT(_dl_,n)%function	;\
__CONCAT(_dl_,n):					;\
	SYSTRAP(c)					;\
	rts						;\
	 nop


	.section	".text"
	.align		4
DL_SYSCALL(close)


	.global		_dl_exit
	.type		_dl_exit%function
_dl_exit:
	SYSTRAP(exit)
1:
	bra 1b
	 nop

DL_SYSCALL(issetugid)
DL_SYSCALL2(_syscall,__syscall)
DL_SYSCALL(munmap)
DL_SYSCALL(mprotect)
DL_SYSCALL(open)
DL_SYSCALL(read)

.L_cerr:
	mov	#-1, r0 
	rts
	 nop

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
	mov	r5, r2			/* fetch new sigset pointer */
	tst	r2, r2			/* check new sigset pointer */
	bf	1f			/* if not null, indirect */
	 mov	#1, r4			/* SIG_BLOCK */
	bra	2f
	 nop
1:	mov.l	@r2, r2			/* fetch indirect ... */
	mov	r2, r5			/* to new mask arg */
2:	mov.l	LSYS_sigprocmask, r0
	trapa	#0x80
	bf	.L_cerr
	 mov	r6, r2			/* fetch old mask requested */
	tst	r2, r2			/* test if old mask requested */
	bt	out
	 mov.l	r0, @r2			/* store old mask */
out:
	xor	r0, r0
	rts
	 nop

	.align	2
LSYS_sigprocmask:
	.long	SYS_sigprocmask


