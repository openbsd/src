/*	$OpenBSD: sigcode_netbsd.s,v 1.1 2001/09/06 10:45:41 art Exp $	*/
/*
 * Copyright (c) 1996-2001 Eduardo Horvath
 * Copyright (c) 1996 Paul Kranenburg
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College.
 *	All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.
 *	All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *	This product includes software developed by Harvard University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by Paul Kranenburg.
 * 4. Neither the name of the University nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 *	@(#)locore.s	8.4 (Berkeley) 12/10/93
 */

	.globl	_C_LABEL(netbsd_sigcode)
	.globl	_C_LABEL(netbsd_esigcode)
_C_LABEL(netbsd_sigcode):
	/*
	 * XXX  the `save' and `restore' below are unnecessary: should
	 *	replace with simple arithmetic on %sp
	 *
	 * Make room on the stack for 64 %f registers + %fsr.  This comes
	 * out to 64*4+8 or 264 bytes, but this must be aligned to a multiple
	 * of 64, or 320 bytes.
	 */
	save	%sp, -CC64FSZ - 320, %sp
	mov	%g2, %l2		! save globals in %l registers
	mov	%g3, %l3
	mov	%g4, %l4
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7
	/*
	 * Saving the fpu registers is expensive, so do it iff it is
	 * enabled and dirty.
	 */
	rd	%fprs, %l0
	btst	FPRS_DL|FPRS_DU, %l0	! All clean?
	bz,pt	%icc, 2f
	 btst	FPRS_DL, %l0		! test dl
	bz,pt	%icc, 1f
	 btst	FPRS_DU, %l0		! test du

	! fpu is enabled, oh well
	stx	%fsr, [%sp + CC64FSZ + BIAS + 0]
	add	%sp, BIAS+CC64FSZ+BLOCK_SIZE, %l0	! Generate a pointer so we can
	andn	%l0, BLOCK_ALIGN, %l0	! do a block store
	stda	%f0, [%l0] ASI_BLK_P
	inc	BLOCK_SIZE, %l0
	stda	%f16, [%l0] ASI_BLK_P
1:
	bz,pt	%icc, 2f
	 add	%sp, BIAS+CC64FSZ+BLOCK_SIZE, %l0	! Generate a pointer so we can
	andn	%l0, BLOCK_ALIGN, %l0	! do a block store
	add	%l0, 2*BLOCK_SIZE, %l0	! and skip what we already stored
	stda	%f32, [%l0] ASI_BLK_P
	inc	BLOCK_SIZE, %l0
	stda	%f48, [%l0] ASI_BLK_P
2:
	membar	#Sync
	rd	%y, %l1			! in any case, save %y
	lduw	[%fp + BIAS + 128], %o0	! sig
	lduw	[%fp + BIAS + 128 + 4], %o1	! code
	call	%g1			! (*sa->sa_handler)(sig,code,scp)
	 add	%fp, BIAS + 128 + 8, %o2	! scp

	/*
	 * Now that the handler has returned, re-establish all the state
	 * we just saved above, then do a sigreturn.
	 */
	btst	3, %l0			! All clean?
	bz,pt	%icc, 2f
	 btst	1, %l0			! test dl
	bz,pt	%icc, 1f
	 btst	2, %l0			! test du

	ldx	[%sp + CC64FSZ + BIAS + 0], %fsr
	add	%sp, BIAS+CC64FSZ+BLOCK_SIZE, %l0	! Generate a pointer so we can
	andn	%l0, BLOCK_ALIGN, %l0	! do a block load
	ldda	[%l0] ASI_BLK_P, %f0
	inc	BLOCK_SIZE, %l0
	ldda	[%l0] ASI_BLK_P, %f16
1:
	bz,pt	%icc, 2f
	 wr	%l1, %g0, %y		! in any case, restore %y
	add	%sp, BIAS+CC64FSZ+BLOCK_SIZE, %l0	! Generate a pointer so we can
	andn	%l0, BLOCK_ALIGN, %l0	! do a block load
	inc	2*BLOCK_SIZE, %l0	! and skip what we already loaded
	ldda	[%l0] ASI_BLK_P, %f32
	inc	BLOCK_SIZE, %l0
	ldda	[%l0] ASI_BLK_P, %f48
2:
	mov	%l2, %g2
	mov	%l3, %g3
	mov	%l4, %g4
	mov	%l5, %g5
	mov	%l6, %g6
	mov	%l7, %g7
	membar	#Sync

	restore	%g0, NETBSD_SYS___sigreturn14, %g1 ! get registers back & set syscall #
	add	%sp, BIAS + 128 + 8, %o0! compute scp
!	andn	%o0, 0x0f, %o0
	t	ST_SYSCALL		! sigreturn(scp)
	! sigreturn does not return unless it fails
	mov	NETBSD_SYS_exit, %g1		! exit(errno)
	t	ST_SYSCALL
_C_LABEL(netbsd_esigcode):
