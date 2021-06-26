/*	$OpenBSD: ldasm.S,v 1.3 2021/06/26 14:50:25 kettenis Exp $ */

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
	 * t0 is the "link map"
	 * t1 is the .got.plt offset
	 */

	/* save parameter/result registers */
	addi	sp, sp, -(10*8)  /* should be aligned well enough */
	sd	ra, (9*8)(sp)
	sd	a0, (0*8)(sp)
	sd	a1, (1*8)(sp)
	sd	a2, (2*8)(sp)
	sd	a3, (3*8)(sp)
	sd	a4, (4*8)(sp)
	sd	a5, (5*8)(sp)
	sd	a6, (6*8)(sp)
	sd	a7, (7*8)(sp)

	/*
	 * no need to save the FP registers as ld.so is compiled such that
	 * it doesn't touch them
	 */

	mv	a0, t0
	srli	a1, t1, 3
	jal	_dl_bind
	mv	t0, a0

	/* restore parameter/result registers */
	ld	a0, (0*8)(sp)
	ld	a1, (1*8)(sp)
	ld	a2, (2*8)(sp)
	ld	a3, (3*8)(sp)
	ld	a4, (4*8)(sp)
	ld	a5, (5*8)(sp)
	ld	a6, (6*8)(sp)
	ld	a7, (7*8)(sp)
	ld	ra, (9*8)(sp)
	addi	sp, sp, (10*8)

	jr	t0
END(_dl_bind_start)

ENTRY(_rtld_tlsdesc)
	RETGUARD_SETUP(_rtld_tlsdesc, x15)
	ld	a0, 8(a0)
	RETGUARD_CHECK(_rtld_tlsdesc, x15)
	ret
END(_rtld_tlsdesc)
