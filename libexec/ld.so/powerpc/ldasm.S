/*	$OpenBSD: ldasm.S,v 1.9 2002/12/09 20:56:34 drahn Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed under OpenBSD by
 *	Dale Rahn.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * #ifdef blrl_never_causes_illegal_access
 * background for the else of this define
 * On the machine this code is being written, on a large portion
 * of the tims this code is executed the blrl instruct causes an
 * illegal instruction, hopefully by touching the page with a load
 * will prevent the execute access from faulting.
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
	mr 20, 3	#argc
	mr 21, 4	#argv
	mr 22, 5	#envp
	mr 23, 6	# ???

	.local	.L___offset_sym
	.type .L___offset_sym,@function
	bl	1f
.L___offset_sym:
	# this instruction never gets executed but can be used
	# to find the virtual address where the page is loaded.
	bl _GLOBAL_OFFSET_TABLE_@local-4
1:
	mflr	5		# this stores where we are (+4)
	lwz	18, 0(5)	# load the instruction at offset_sym
				# it contains an offset to the location
				# of the GOT.

	rlwinm 18,18,0,8,30	# mask off the offset portion of the instr.

	/*
	 * these adds effectively calculate the value the
	 * bl _GLOBAL_OFFSET_TABLE_@local-4
	 * operation that would be below would calulate.
	 */
	add	28, 18, 5
	mr	6, 5 	# save offset for later use

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

	# calculate where we want to be (via the got)
	bl _GLOBAL_OFFSET_TABLE_@local-4
	mflr 28


	lwz	4, .L___offset_sym@got(28)
	mr	6, 4		# make copy of register for debugging


	sub	4, 5, 4		# calculate offset (arg1 for _dl_boot)
	mr	17, 4


	subi	3, 21, 4	# Get stack pointer (arg0 for _dl_boot).
	addi	4, 1, 8		# dl_data

	bl	_dl_boot_bind@local

	mr	3, 21		# argv
	mr	4, 22		# envp
	mr	5, 17		# loff
	addi	6, 1, 8		# dl_data

	bl	_dl_boot@local

	mtctr 3			# put return value into ctr to execute

	# get back the squirreled away the arguments for main
	mr 3, 20
	mr 4, 21
	mr 5, 22
	mr 6, 23


	mtlr 27
	lwz	1, 0(1)		# Restore stack pointer.
	bctr			# Go execute the 'real' program.

ENTRY(_dl_bind_start)
	stwu	1,-64(1)

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

	addi	1,1,64
	bctr
