/*	$OpenBSD: ldasm.S,v 1.3 2020/07/16 21:18:09 kettenis Exp $ */

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

	.section .boot.text,"ax",@progbits
	.align	2
	.globl	_dl_start
	.type	_dl_start,@function
_dl_start:
	bl	1f
1:
	mflr	%r30
	addis   %r2, %r30, .TOC.-1b@ha
	addi    %r2, %r2, .TOC.-1b@l

	stdu	%r1, (-64 -((AUX_entry+3)*8))(%r1)	# Some space.

	# squirrel away the arguments for main
	mr	%r20, %r3	#argc
	mr	%r21, %r4	#argv
	mr	%r22, %r5	#envp
	mr	%r23, %r6	# ???
	nop

	addis	%r18, %r2,  _DYNAMIC@toc@ha
	addi	%r18, %r18, _DYNAMIC@toc@l

	ld	%r4,  -0x8000(%r2) # First entry of TOC is TOC base
	sub	%r4,  %r2, %r4	# determine load offset

	mr	%r17, %r4		# save for _dl_boot

	subi	%r3, %r21, 8	# Get stack pointer (arg0 for _dl_boot).
	addi	%r4, %r1, 32		# dl_data
	mr	%r5, %r18		# dynamicp

	bl	_dl_boot_bind

	mr	%r3, %r21		# argv
	mr	%r4, %r22		# envp
	mr	%r5, %r17		# loff
	addi	%r6, %r1, 32		# dl_data

	bl	_dl_boot

	mtctr	%r3		# put return value into ctr to execute

	# get back the squirreled away the arguments for main
	mr	%r3, %r20
	mr	%r4, %r21
	mr	%r5, %r22
	mr	%r6, %r23

	addis	%r7, %r2, _dl_dtors@toc@ha
	addi	%r7, %r7, _dl_dtors@toc@l

	mtlr	%r27
	ld	%r1, 0(%r1)		# Restore stack pointer.
	bctr			# Go execute the 'real' program.
END(_dl_start)

ENTRY(_dl_bind_start)
	# r0 contains offset, do not overwrite 
	# r2 ld.so toc is loaded on entry to this function.
	mflr	%r12
	std	%r12,16(%r1)		# save	lr

	stdu	1,-104(%r1)

	std	%r3,32(%r1)		# save	r3-r10, C calling convention
	std	%r4,40(%r1)		# r13 - r31 are preserved by called code
	std	%r5,48(%r1)
	std	%r6,56(%r1)
	std	%r7,64(%r1)
	std	%r8,72(%r1)
	std	%r9,80(%r1)
	std	%r10,88(%r1)

	mr	%r3,%r11		# obj
	mr	%r4,%r0			# relidx
	bl	_dl_bind		# _rtld_bind(obj, relidx)
	mtctr	%r3
	mr	%r12, %r3

	ld	%r3,32(%r1)
	ld	%r4,40(%r1)
	ld	%r5,48(%r1)
	ld	%r6,56(%r1)
	ld	%r7,64(%r1)
	ld	%r8,72(%r1)
	ld	%r9,80(%r1)
	ld	%r10,88(%r1)

	addi	%r1,%r1,104

	ld	%r0,16(%r1)		# restore lr
	mtlr	%r0

	bctr
END(_dl_bind_start)
