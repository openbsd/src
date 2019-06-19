/*	$OpenBSD: ldasm.S,v 1.42 2018/11/22 21:37:30 guenther Exp $ */

/*
 * Copyright (c) 2001 Niklas Hallqvist
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
/*
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>

#define AUX_entry 9

/* Not really a leaf... but we are special.  */
LEAF_NOPROFILE(_dl_start, 0)
	.set	noreorder
	br	pv, L1
L1:
	LDGP(pv)

	mov	a0, s0		/* save arg */

	/* relocate ourself. */
	br	s2, L2		/* get our PC */
L2:	ldiq	s3, L2		/* get where the linker thought we were */

	subq	s2, s3, s2
	lda	t5, _DYNAMIC
	addq	s2, t5, s6

	/* allocate stack */
	lda	sp, -((AUX_entry + 1) * 8)(sp)

	mov	s0, a0
	mov	s2, s1		/* relocation displacement */
	ldq	a2, 0(a0)	/* argc */
	lda	a3, 8(a0)	/* argv */
	mov	a3, s3
	lda	t3, 1(a2)
	sll	t3, 3, t3
	addq	a3, t3, a4	/* envp */
	mov	a4, s4
	mov	a5, s5
	lda	s2, 0(sp)
	mov	s2, a1
	mov	s6, a2		/* &_DYNAMIC */
	/* we can't use CALL here as gp would not be set correctly by ldgp,
	   due to the GOT not being relocated yet. */
	lda	pv, _dl_boot_bind
	addq	s1, pv, pv
	bsr	ra, _dl_boot_bind
	mov	s3, a0		/* **argv  */
	mov	s4, a1		/* **envp  */
	mov	s1, a2		/* loff    */
	mov	s2, a3		/* dl_data */
	CALL(_dl_boot)
	mov	s0, a0		/* stack */
	lda	a1, _dl_dtors	/* cleanup */
	mov	v0, pv
	jsr	ra, (pv)
END(_dl_start)

/*
 * Lazy binding entry point, called via secure (read-only) PLT.
 */
NESTED_NOPROFILE(_dl_bind_start, 0, 160, ra, 0, 0)
	.set	noat
	/* at_reg and t11 already used by PLT code. */

	/*
	 * Allocate stack frame and preserve all registers that the caller
	 * would have normally saved themselves.
	 */
	lda	sp, -160(sp)
	stq	ra, 0(sp)
	stq	v0, 8(sp)
	stq	t0, 16(sp)
	stq	t1, 24(sp)
	stq	t2, 32(sp)
	stq	t3, 40(sp)
	stq	t4, 48(sp)
	stq	t5, 56(sp)
	stq	t6, 64(sp)
	stq	t7, 72(sp)
	stq	a0, 80(sp)
	stq	a1, 88(sp)
	stq	a2, 96(sp)
	stq	a3, 104(sp)
	stq	a4, 112(sp)
	stq	a5, 120(sp)
	stq	t8, 128(sp)
	stq	t9, 136(sp)
	stq	t10, 144(sp)
	stq	gp, 152(sp)

	/*
	 * Load our global pointer.  Note, can't use pv, since it is
	 * already used by the PLT code.
	 */
	br	t0, 1f
1:	LDGP(t0)

	/* Set up the arguments for _dl_bind. */
	mov	at_reg, a0	/* object */
	mov	t11, a1		/* reloff as computed by the plt resolver */
	CALL(_dl_bind)

	/* Move the destination address into position. */
	mov	v0, pv

	/* Restore program registers. */
	ldq	ra, 0(sp)
	ldq	v0, 8(sp)
	ldq	t0, 16(sp)
	ldq	t1, 24(sp)
	ldq	t2, 32(sp)
	ldq	t3, 40(sp)
	ldq	t4, 48(sp)
	ldq	t5, 56(sp)
	ldq	t6, 64(sp)
	ldq	t7, 72(sp)
	ldq	a0, 80(sp)
	ldq	a1, 88(sp)
	ldq	a2, 96(sp)
	ldq	a3, 104(sp)
	ldq	a4, 112(sp)
	ldq	a5, 120(sp)
	ldq	t8, 128(sp)
	ldq	t9, 136(sp)
	ldq	t10, 144(sp)
	ldq	gp, 152(sp)
	/* XXX LDGP? */

	/* Pop the stack frame and turn control to the destination. */
	lda	sp, 160(sp)
	jmp	zero, (pv)
END(_dl_bind_start)
