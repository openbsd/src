/* $OpenBSD: sigprocmask.S,v 1.7 2002/10/07 04:47:12 drahn Exp $ */

/*
 * Copyright (c) 1996 Dale Rahn
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
 */

/* sigprocmask(int how, const sigset_t *set, sigset_t *oset); */

#include "SYS.h"

	.text
PREFIX(sigprocmask)
	stwu	1, -16(1)
	stw	5, 12(1)
	
	/* check set (new mask value) for null, in which case
	   fiddle arguments */
	cmpwi	4, 0
	bne+	.L_load_set
	addi	3, 0, 1	/* how = SIG_BLOCK, new mask already 0 */
	b .L_do_call
.L_load_set:
	lwz	4, 0(4)	/* get new mask */
.L_do_call:
	
	sc
	
	/* didnt work? */
	cmpwi	0, 0
	beq+	.L_sigprocmask_ok
	b	PIC_PLT(_ASM_LABEL(__cerror))

.L_sigprocmask_ok:
	lwz	5, 12(1)
	cmpwi	5, 0
	beq+	.L_sigprocmask_done
	stw	3, 0(5)
.L_sigprocmask_done:
	li	3, 0
	addi	1, 1, 16
	blr
