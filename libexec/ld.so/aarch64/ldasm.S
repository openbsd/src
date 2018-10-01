/*	$OpenBSD: ldasm.S,v 1.7 2018/10/01 22:53:48 mortimer Exp $ */

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

ENTRY(_dl_start)
	mov	x29, sp
	// need to worry about alignment, I think not?
	mov	x19, sp
	sub	sp, sp, #8+8+DL_DATA_SIZE	// dl_data size
	add	x20, sp, #4			// dl_data
	mov	x21, lr				// save old lr

	mov	x0, x29				// original stack
	mov	x1, x20				// dl_data

	adrp	x2, _DYNAMIC			/* &_DYNAMIC */
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
	 * x16 is pointer to pltgot[2]
	 * x17 is available as scratch register
	 * return address and pointer to pltgot entry for this
	 * relocation are on the stack
	 */
	mov x17, sp

	// save parameter/result registers
	stp	x0, x1, [sp, #-16]!
	stp	x2, x3, [sp, #-16]!
	stp	x4, x5, [sp, #-16]!
	stp	x6, x7, [sp, #-16]!
	stp	x8, xzr, [sp, #-16]!

	/*
	 * no need to save v0-v9 as ld.so is compiled with
	 * -march=armv8-a+nofp+nosimd and therefore doesn't touch the
	 * SIMD and Floating-Point registers
	 */

	ldr	x0, [x16, #-8]		// object
	ldr	x2, [x17]
	sub	x1, x2, x16
	sub	x1, x1, #8
	lsr	x1, x1, #3		// relidx
	bl	_dl_bind
	mov	x17, x0

	// restore parameter/result registers
	ldp	x8, xzr, [sp], #16
	ldp	x6, x7, [sp], #16
	ldp	x4, x5, [sp], #16
	ldp	x2, x3, [sp], #16
	ldp	x0, x1, [sp], #16

	// restore LR saved by PLT stub	
	ldp	xzr, x30, [sp], #16
	br	x17

ENTRY(_rtld_tlsdesc)
	RETGUARD_SETUP(_rtld_tlsdesc, x15)
	ldr	x0, [x0, #8]
	RETGUARD_CHECK(_rtld_tlsdesc, x15)
	ret
