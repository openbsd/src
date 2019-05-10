/*	$OpenBSD: ldasm.S,v 1.28 2019/05/10 13:29:21 guenther Exp $ */

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

	.section .boot.text,"ax",%progbits
	_ALIGN_TEXT
	.globl	_dl_start
	.type	_dl_start,#function
_dl_start:
	mov	fp, sp
	mov	r5, sp
	mov	lr, r6				@ save lr
	sub	sp, sp, #4+4+DL_DATA_SIZE
	add	r7, sp, #4			@ dl_data

	mov	r0, fp				@ original stack
	mov	r1, r7				@ dl_data

	ldr	r8, .L_GOT			@ calculate address of GOT...
1:	add	r8, pc, r8			@ into r8

	ldr	r2, .L__DYNAMIC			@ &_DYNAMIC
	add	r2, r2, r8

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

	mov	r1, r0
	ldr	r0, .L_dl_dtors
	add	r0, r0, r8
	mov	pc, r1
.L_GOT:
	.long	_GLOBAL_OFFSET_TABLE_-(1b+8)
.L__DYNAMIC:
	.long	_DYNAMIC(GOTOFF)
.L_dl_dtors:
	.long	_dl_dtors(GOTOFF)
END(_dl_start)


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
END(_dl_bind_start)
