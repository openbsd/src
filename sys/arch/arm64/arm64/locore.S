/* $OpenBSD: locore.S,v 1.18 2017/06/04 14:21:49 patrick Exp $ */
/*-
 * Copyright (c) 2012-2014 Andrew Turner
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: head/sys/arm64/arm64/locore.S 282867 2015-05-13 18:57:03Z zbb $
 */

#include "assym.h"
#include <sys/syscall.h>
#include <machine/asm.h>
#include <machine/param.h>

#define	VECT_EMPTY	\
	.align 7;	\
	1:	b	1b

	.align 11
	.globl hyp_vectors
hyp_vectors:
	VECT_EMPTY	/* Synchronous EL2t */
	VECT_EMPTY	/* IRQ EL2t */
	VECT_EMPTY	/* FIQ EL2t */
	VECT_EMPTY	/* Error EL2t */

	VECT_EMPTY	/* Synchronous EL2h */
	VECT_EMPTY	/* IRQ EL2h */
	VECT_EMPTY	/* FIQ EL2h */
	VECT_EMPTY	/* Error EL2h */

	VECT_EMPTY	/* Synchronous 64-bit EL1 */
	VECT_EMPTY	/* IRQ 64-bit EL1 */
	VECT_EMPTY	/* FIQ 64-bit EL1 */
	VECT_EMPTY	/* Error 64-bit EL1 */

	VECT_EMPTY	/* Synchronous 32-bit EL1 */
	VECT_EMPTY	/* IRQ 32-bit EL1 */
	VECT_EMPTY	/* FIQ 32-bit EL1 */
	VECT_EMPTY	/* Error 32-bit EL1 */

	.align 3
	.globl abort
abort:
	b abort

	.data
	.global _C_LABEL(esym)
_C_LABEL(esym): .xword   _C_LABEL(end)

data_align_pad:
	.space 32
	.align 12 /* 4KiB aligned */
	/*
	 * 3 initial tables (in the following order):
	 *           L2 for kernel (High addresses)
	 *           L1 for kernel
	 *           L1 for user   (Low addresses)
	 */
	.globl	pagetable
pagetable:
	.space	PAGE_SIZE * 2	// allocate 2 pages for pmapvp2
pagetable_l1_ttbr1:
	.space	PAGE_SIZE * 2	// allocate 2 pages for pmapvp1
pagetable_l1_ttbr0:
	.space	PAGE_SIZE * 2	// allocate 2 pages, reused later in pmap
pagetable_l0_ttbr0:
	.space	PAGE_SIZE
	.globl	pagetable_end
pagetable_end:
	.bss
	.align	4
	.globl initstack
initstack:
	.space	USPACE
	.globl initstack_end
initstack_end:

	.text
ENTRY(sigcode)
	mov	x0, sp
	add	x0, x0, #SF_SC

1:
	mov	x8, #SYS_sigreturn
	svc	0
	.globl  _C_LABEL(sigcoderet)
_C_LABEL(sigcoderet):

	/* sigreturn failed, exit */
	mov	x8, #SYS_exit
	svc	0

	b	1b
END(sigcode)
	/* This may be copied to the stack, keep it 16-byte aligned */
	.align  3
	.global _C_LABEL(esigcode)
_C_LABEL(esigcode):

	.globl	sigfill
sigfill:
	.word	0xa000f7f0		/* FIXME: illegal on all cpus? */
esigfill:

	.data
	.globl	sigfillsiz
sigfillsiz:
	.word	esigfill - sigfill

	.text
