/*	$OpenBSD: ldasm.S,v 1.3 2013/04/05 12:58:03 kurt Exp $ */

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
/*-
 * Copyright (c) 1999, 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein and by Charles M. Hannum.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>
#include <sys/syscall.h>
#include <SYS.h>

/*
 * ld.so entry point.
 * On entry: %sp points to the kernel argument struct (argv, argv, environ).
 * The environment pointers list is followed by an array of AuxInfo
 * structs filled by the kernel.
 */
#define	DL_DATA_SIZE	(4 * 16)	/* 16 * sizeof(ELF_Addr) */
ENTRY(_dl_start)
	/*
	 * Get some room for the contiguous AUX pointers array.
	  */
	subl	#DL_DATA_SIZE, %sp

	/*
	 * Invoke _dl_boot_bind
	 */
	pea	0(%sp)			| array base
	pea	(4 + DL_DATA_SIZE)(%sp)	| kernel args
	jbsr	_dl_boot_bind@PLTPC
	addql	#8, %sp

	movl	(4 * 7)(%sp), %d1	| ldoff
	leal	DL_DATA_SIZE(%sp), %a0
	movl	(%a0)+, %d0		| argc, a0 points to argv
	lea	4(%a0,%d0.l*4), %a1	| envp

	pea	0(%sp)			| array base
	movl	%d1, -(%sp)		| ldoff
	movl	%a1, -(%sp)		| envp
	movl	%a0, -(%sp)		| argv
	jbsr	_dl_boot@PLTPC
	addl	#16, %sp

	addl	#DL_DATA_SIZE, %sp	| restore sp
	moveal	%d0, %a0
	clrl	%d1
	moveal	%d1, %a1		| cleanup as expected by CSU
	jmp	(%a0)

	rts

ENTRY(_dl_bind_start)
	moveml	%d0-%d1/%a0-%a1,-(%sp)	/* preserve caller-saved registers */
	movel	20(%sp),-(%sp)		/* push reloff */
	movel	(16+4)(%sp),-(%sp)	/* push obj */
	jbsr	_dl_bind@PLTPC		/* %d0 = _dl_bind(obj, reloff) */
	addql	#8,%sp			/* pop args */
	movel	%d0,20(%sp)		/* write fake `return' address over
					   reloff */
	moveml	(%sp)+,%d0-%d1/%a0-%a1	/* restore caller-saved registers */
	addql	#4,%sp			/* skip obj */
	rts				/* `return' right into function */

/* ld.so SYSCALLS */

#ifdef __STDC__
#define	__CONCAT(x,y)	x ## y
#else
#define	__CONCAT(x,y)	x/**/y
#endif

#define DL_SYSCALL(n) DL_SYSCALL2(n,n)
#define DL_SYSCALL2(n,c)				\
	.global		__CONCAT(_dl_,n)		;\
	.type		__CONCAT(_dl_,n)%function	;\
__CONCAT(_dl_,n):					;\
	__DO_SYSCALL(c)					;\
	jcs	.L_cerr	

#define DL_SYSCALL2_NOERR(n,c)				\
	.global		__CONCAT(_dl_,n)		;\
	.type		__CONCAT(_dl_,n)%function	;\
__CONCAT(_dl_,n):					;\
	__DO_SYSCALL(c)

	.section	".text"
	.align		2
DL_SYSCALL(close)
	rts

	.global		_dl_exit
	.type		_dl_exit%function
_dl_exit:
	__DO_SYSCALL(exit)
1:
	jra 1b

DL_SYSCALL(issetugid)
	rts

DL_SYSCALL2(_syscall,__syscall)
	movl	%d0, %a0	/* ??? */
	rts

DL_SYSCALL(munmap)
	rts

DL_SYSCALL(mprotect)
	rts

DL_SYSCALL(open)
	rts

DL_SYSCALL(read)
	rts

DL_SYSCALL(write)
	rts

DL_SYSCALL(fstat)
	rts

DL_SYSCALL(fcntl)
	rts

DL_SYSCALL(gettimeofday)
	rts

DL_SYSCALL(readlink)
	rts

DL_SYSCALL(lstat)
	rts

DL_SYSCALL2(getcwd,__getcwd)
	rts

DL_SYSCALL2(sysctl,__sysctl)
	rts

DL_SYSCALL(getdirentries)
	rts

	.global		_dl_sigprocmask
	.type		_dl_sigprocmask%function
_dl_sigprocmask:
	tstl	%sp@(8)			/* check new sigset pointer */
	jne	gotptr			/* if not null, indirect */
/*	movl	#0,%sp@(8)	*/	/* null mask pointer: block empty set */
	movl	#1,%sp@(4)		/* SIG_BLOCK */
	jra	doit
gotptr:
	movl	%sp@(8),%a0
	movl	%a0@,%sp@(8)		/* indirect to new mask arg */
doit:
	movl	#SYS_sigprocmask,%d0
	trap	#0
	jcs	.L_cerr
	tstl	%sp@(12)		/* test if old mask requested */
	jeq	out
	movl	%sp@(12),%a0
	movl	%d0,%a0@		/* store old mask */
out:
	clrl	%d0
	rts

.L_cerr:
	movl	#-1, %d0 
	rts

