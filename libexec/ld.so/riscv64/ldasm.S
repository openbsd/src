/*	$OpenBSD: ldasm.S,v 1.2 2021/05/16 16:00:50 drahn Exp $ */

/*
 * Copyright (c) 2016,2021 Dale Rahn <drahn@openbsd.org>
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

#define DL_DATA_SIZE	(16 * 8)	/* needs to be 8(16?) byte aligned */
#include <machine/asm.h>
#include <sys/syscall.h>

	.option norelax
	.section .boot.text,"ax",@progbits
	_ALIGN_TEXT
	.globl	_dl_start
	.type	_dl_start,@function
_dl_start:
	mv	a0, sp
	mv	fp, sp

	addi	sp, sp, -(8+8+DL_DATA_SIZE)	// dl_data size
	addi	s10, sp, 8			// dl_data

	mv	a1, s10				// dl_data

1:	auipc	a2, %pcrel_hi(_DYNAMIC)		/* &_DYNAMIC */
	addi	a2, a2, %pcrel_lo(1b)

	call	_dl_boot_bind

	ld	a0, (fp)			// load argc
	addi    a1, fp, 0x0008			// argv
	slli    a2, a0, 0x3
	add     a2, a1, a2
	addi    a2, a2, 0x0008			// compute envp into a2

	// _dl_boot(argv, envp, loff, dl_data)
	mv	a0, a1				// argv
	mv	a1, a2				// envp
	ld	a2, (7*8)(s10)			// loff from dl_data
	mv	a3, s10				// dl_data

	call	_dl_boot

	mv	sp, fp				// move stack back
	mv	fp, zero			// clear frame back pointer

2:	auipc	a3, %pcrel_hi(_dl_dtors)		/* cleanup */
	addi	a3, a3, %pcrel_lo(2b)

	jr	a0
END(_dl_start)

ENTRY(_dl_bind_start)
	/*
	 * x16 is pointer to pltgot[2]
	 * x17 is available as scratch register
	 * return address and pointer to pltgot entry for this
	 * relocation are on the stack
	 */
	mv	x17, sp

	/* save parameter/result registers */
	addi	sp, sp, -(16*8)  /* should be aligned well enough */
	sd	t1, ( 2*8)(sp)
	sd	t2, ( 3*8)(sp)
	sd	a0, ( 4*8)(sp)
	sd	a1, ( 5*8)(sp)
	sd	a2, ( 6*8)(sp)
	sd	a3, ( 7*8)(sp)
	sd	a4, ( 8*8)(sp)
	sd	a5, ( 9*8)(sp)
	sd	a6, (10*8)(sp)
	sd	a7, (11*8)(sp)
	sd	t3, (12*8)(sp)
	sd	t4, (13*8)(sp)
	sd	t5, (14*8)(sp)
	sd	t6, (15*8)(sp)

	/* what about float registers !?! */
	/*
	 * no need to save v0-v9 as ld.so is compiled with
	 * -march=armv8-a+nofp+nosimd and therefore doesn't touch the
	 * SIMD and Floating-Point registers
	 */

	mv	a0, t0
	slli	a1, t1, 1
	add	a1, a1, t1
	jal	_dl_bind
	nop
	mv	t0, a0

	// restore parameter/result registers
	ld	t1, ( 2*8)(sp)
	ld	t2, ( 3*8)(sp)
	ld	a0, ( 4*8)(sp)
	ld	a1, ( 5*8)(sp)
	ld	a2, ( 6*8)(sp)
	ld	a3, ( 7*8)(sp)
	ld	a4, ( 8*8)(sp)
	ld	a5, ( 9*8)(sp)
	ld	a6, (10*8)(sp)
	ld	a7, (11*8)(sp)
	ld	t3, (12*8)(sp)
	ld	t4, (13*8)(sp)
	sd	t5, (14*8)(sp)
	sd	t6, (15*8)(sp)
	add	sp, sp, (16*8)

	// restore LR saved by PLT stub	
	// XXX - correct?
	ld	ra, 16(sp)
	add	sp, sp, 16
	jr	t0
END(_dl_bind_start)

ENTRY(_rtld_tlsdesc)
	RETGUARD_SETUP(_rtld_tlsdesc, x15)
	ld	a0, 8(a0)
	RETGUARD_CHECK(_rtld_tlsdesc, x15)
	ret
END(_rtld_tlsdesc)
