/*	$OpenBSD: ldasm.S,v 1.1 2004/02/07 06:00:49 drahn Exp $ */

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
#include <arm/swi.h>

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
	/* STUB */


/* ld.so SYSCALLS */

	.section	".text"
	.align		4
	.global		_dl_close
	.type		_dl_close%function
_dl_close:
	swi	SWI_OS_NETBSD|SYS_close
	bcs	.L_cerr
	mov	pc, lr
	
	.global		_dl_exit
	.type		_dl_exit%function
_dl_exit:
	swi	SWI_OS_NETBSD|SYS_exit
	1:
		b 1b

	.global		_dl_issetugid
	.type		_dl_issetugid%function
_dl_issetugid:
	swi	SWI_OS_NETBSD|SYS_issetugid
	bcs	.L_cerr
	mov	pc, lr
	

	.global		_dl__syscall
	.type		_dl__syscall%function
_dl__syscall:
	swi	SWI_OS_NETBSD|SYS___syscall
	bcs	.L_cerr
	mov	pc, lr
	
	.global		_dl_munmap
	.type		_dl_munmap%function
_dl_munmap:
	swi	SWI_OS_NETBSD|SYS_munmap
	bcs	.L_cerr
	mov	pc, lr
	
	.global		_dl_mprotect
	.type		_dl_mprotect%function
_dl_mprotect:
	swi	SWI_OS_NETBSD|SYS_mprotect
	bcs	.L_cerr
	mov	pc, lr
	
	.global		_dl_open
	.type		_dl_open%function
_dl_open:
	swi	SWI_OS_NETBSD|SYS_open
	bcs	.L_cerr
	mov	pc, lr
	
	.global		_dl_read
	.type		_dl_read%function
_dl_read:
	swi	SWI_OS_NETBSD|SYS_read
	bcs	.L_cerr
	mov	pc, lr
	
	.global		_dl_write
	.type		_dl_write%function
_dl_write:
	swi	SWI_OS_NETBSD|SYS_write
	bcs	.L_cerr
	mov	pc, lr
	
	.global		_dl_stat
	.type		_dl_stat%function
_dl_stat:
	swi	SWI_OS_NETBSD|SYS_stat
	bcs	.L_cerr
	mov	pc, lr
	
	.global		_dl_fstat
	.type		_dl_fstat%function
_dl_fstat:
	swi	SWI_OS_NETBSD|SYS_fstat
	bcs	.L_cerr
	mov	pc, lr
	
	.global		_dl_fcntl
	.type		_dl_fcntl%function
_dl_fcntl:
	swi	SWI_OS_NETBSD|SYS_fcntl
	bcs	.L_cerr
	mov	pc, lr

	.global		_dl_sysctl
	.type		_dl_sysctl%function
_dl_sysctl:
	swi	SWI_OS_NETBSD|SYS___sysctl
	bcs	.L_cerr
	mov	pc, lr
	
	.global		_dl_getdirentries
	.type		_dl_getdirentries%function
_dl_getdirentries:
	swi	SWI_OS_NETBSD|SYS_getdirentries
	bcs	.L_cerr
	mov	pc, lr
	
	.global		_dl_sigprocmask
	.type		_dl_sigprocmask%function
_dl_sigprocmask:
	teq	r1, #0
	moveq	r0, #1
	moveq	r1, #0
	ldrne	r1, [r1]
	swi	SWI_OS_NETBSD|SYS_sigprocmask
	bcs	.L_cerr
	teq	r2, #0
	strne	r0, [r2]
	mov	r0, #0
	mov	pc, lr
	
.L_cerr:
	mov	r0, #0
	sub	r0, r0, #1
	mov	pc, lr
