/*	$OpenBSD: ldasm.S,v 1.1 2004/08/11 17:11:45 pefo Exp $ */

/*
 * Copyright (c) 1998-2002 Opsycon AB, Sweden.
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

#include <machine/asm.h>

/* Stack at this stage is:
 * struct stack {
 *	int	kargc;
 *	char	*kargv[1];	size depends on kargc
 *	char	kargstr[1];	size varies
 *	char	kenvstr[1];	size varies
 * };
 */

LEAF(_dl_start)		/* Not really LEAF, but we simplify */
	addu	sp, -64		# Some space.
	.cprestore 12

	la	a1, 1f
	bgezal	zero, 1f
1:
	subu	s0, ra, a1	# This is the load offset
	addu	a0, sp, 64	# Where stack info is.
	addu	a1, sp, 16	# Where fast AUX info will be.
	la	t9, _dl_boot_bind
	addu	t9, s0
	jalr	t9		# Relocate ourself.

	lw	a3, 64(sp)	# argc
	addu	a0, sp, 68	# argv
	addi	a1, a0, 4
	sll	a3, a3, 2
	addu	a1, a3
	addu	a3, sp, 16	# Where fast AUX info will be.
	move	a2, s0		# Load offset
	jal	_dl_boot	# Go do the linking.

	addu	sp, 64		# Restore stack pointer.
	move	t9, v0		# Entry address from _dl_boot.
	j	t9		# Go execute the 'real' program.
END(_dl_start)

LEAF(_dl__syscall)
	li	v0, 0		# Indirect syscall.
	bne	a3, zero, 1f
	j	ra
1:
	li	v0, -1
	j	ra
END(_dl__syscall)

	.globl	_dl_rt_resolve
	.ent	_dl_rt_resolve, 0
_dl_rt_resolve:

/* XXX Fix when lazy binding works */
	.end	_dl_rt_resolve
