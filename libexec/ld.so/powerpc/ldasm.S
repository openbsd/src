/*	$OpenBSD: ldasm.S,v 1.22 2015/06/01 19:55:20 miod Exp $ */

/*
 * Copyright (c) 1999 Dale Rahn
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

#define AUX_entry 9

#include <machine/asm.h>
#include <sys/syscall.h>

ENTRY(_dl_start)
	mr	19, 1
	stwu	1, (-16 -((AUX_entry+3)*4))(1)	# Some space.

	mflr	27		/* save off old link register */
	stw	27, 4(19)	/* save in normal location */

	# squirrel away the arguments for main
	mr	20, 3	#argc
	mr	21, 4	#argv
	mr	22, 5	#envp
	mr	23, 6	# ???

	bl	1f
	# this instruction never gets executed but can be used
	# to find the virtual address where the page is loaded.
	bl _GLOBAL_OFFSET_TABLE_@local-4
	bl _DYNAMIC@local
1:
	mflr	5		# this stores where we are (+4)
	lwz	18, 0(5)	# load the instruction at offset_sym
				# it contains an offset to the location
				# of the GOT.

	rlwinm	18,18,0,8,30	# mask off the offset portion of the instr.

	/*
	 * these adds effectively calculate the value the
	 * bl _GLOBAL_OFFSET_TABLE_@local-4
	 * operation that would be below would calculate.
	 */
	add	28, 18, 5
	mr	6, 5		# save offset for later use

	/* mprotect GOT-4 for correct execution of blrl instruction */
	li	0, SYS_mprotect
	mr	3, 28
	li	4, 4
	li	5, 7 /* (PROT_READ|PROT_WRITE|PROT_EXEC) */
	sc

	mr	5, 6

	li	0, 0
	dcbf	5, 18
	sync
	isync
	icbi	5, 18		# make certain that the got table addr is
				# not in the icache
	sync
	isync

	/* This calculates the address of _DYNAMIC the same way
	 * that the GLOBAL_OFFSET_TABLE was calculated.
	 */
	lwz	18, 4(5)
	rlwinm	18,18,0,8,30	# mask off the offset portion of the instr.
	add	8, 18, 5	# address of _DYNAMIC (arg6 for _dl_boot)
	addi	18, 8, 4	# correction.
	lwz	4, 4(28)	# load address of _DYNAMIC according to got.
	sub	4, 18, 4	# determine load offset

	mr	17, 4		# save for _dl_boot


	subi	3, 21, 4	# Get stack pointer (arg0 for _dl_boot).
	addi	4, 1, 8		# dl_data
	mr	5, 18		# dynamicp

	bl	_dl_boot_bind@local

	mr	3, 21		# argv
	mr	4, 22		# envp
	mr	5, 17		# loff
	addi	6, 1, 8		# dl_data

	bl	_dl_boot@local

	mtctr	3		# put return value into ctr to execute

	# get back the squirreled away the arguments for main
	mr	3, 20
	mr	4, 21
	mr	5, 22
	mr	6, 23

	bl	_GLOBAL_OFFSET_TABLE_@local-4
	mflr	31
	lwz	7, _dl_dtors@got(31)

	mtlr	27
	lwz	1, 0(1)		# Restore stack pointer.
	bctr			# Go execute the 'real' program.

ENTRY(_dl_bind_start)
	stwu	1,-72(1)

	stw	0,8(1)		# save r0 - cerror ;-)
	mflr	0
	stw	0,68(1)		# save	lr

	stw	3,12(1)		# save	r3-r10, C calling convention
	stw	4,20(1)		# r13 - r31 are preserved by called code
	stw	5,24(1)
	stw	6,28(1)
	stw	7,32(1)
	stw	8,36(1)
	stw	9,40(1)
	stw	10,44(1)

	mr	3,12		# obj
	mr	4,11		# reloff
	bl	_dl_bind@plt	#	_rtld_bind(obj,	reloff)
	mtctr	3

	lwz	3,12(1)
	lwz	4,20(1)
	lwz	5,24(1)
	lwz	6,28(1)
	lwz	7,32(1)
	lwz	8,36(1)
	lwz	9,40(1)
	lwz	10,44(1)

	lwz	0,68(1)		# restore lr
	mtlr	0
	lwz	0,8(1)

	addi	1,1,72
	bctr

#define	DL_SYSCALL(n)		DL_SYSCALL2(n,n)
#define	DL_SYSCALL_NOERR(n)	DL_SYSCALL2_NOERR(n,n)
#define	DL_SYSCALL2(n,c)						\
ENTRY(_dl_##n)								\
	li	0, SYS_##c;						\
	sc;								\
	cmpwi	0, 0;							\
	beqlr+;								\
	b	_dl_cerror
#define	DL_SYSCALL2_NOERR(n,c)						\
ENTRY(_dl_##n)								\
	li	0, SYS_##c;						\
	sc;								\
	blr

_dl_cerror:
	neg	3, 3
	blr

ENTRY(_dl_sigprocmask)
	/* _dl_sigprocmask does not support NULL as the new mask */
#if 0
	cmpwi	4, 0
	bne+	1f
	li	3, 1		/* SIG_BLOCK */
	b	2f
1:
#endif
	lwz	4, 0(4)
2:
	li	0, SYS_sigprocmask
	sc
	cmpwi	0, 0
	bne-	_dl_cerror
	cmpwi	5, 0
	beq-	1f
	stw	3, 0(5)
1:
	li	3, 0
	blr

DL_SYSCALL(close)
DL_SYSCALL_NOERR(exit)
DL_SYSCALL(fstat)
DL_SYSCALL2(getcwd,__getcwd)
DL_SYSCALL(getdents)
DL_SYSCALL(getentropy)
DL_SYSCALL(sendsyslog)
DL_SYSCALL(gettimeofday)
DL_SYSCALL_NOERR(issetugid)
DL_SYSCALL(lstat)
DL_SYSCALL(mmap)
DL_SYSCALL(mprotect)
DL_SYSCALL(munmap)
DL_SYSCALL(open)
DL_SYSCALL(read)
DL_SYSCALL(readlink)
DL_SYSCALL2(_syscall,__syscall)
DL_SYSCALL2(sysctl,__sysctl)
DL_SYSCALL(utrace)
DL_SYSCALL(write)
