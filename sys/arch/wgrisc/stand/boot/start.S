/*	$OpenBSD: start.S,v 1.2 1997/07/21 06:58:13 pefo Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
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
 *	Per Fogelstrom.
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

#define ABICALLS

#include <machine/regdef.h>
#include <machine/asm.h>

/*
 * Frame required for the debugger (if we have any)
 */
#define START_FRAME	((4 * 4) + 4 + 4)

	.globl	__start
__start:
	.set	noreorder
	la      gp, _gp
	la	sp, __start - START_FRAME		# Stack below program
	sw	zero, START_FRAME - 4(sp)	# Zero out old ra for debugger
	sw	zero, START_FRAME - 8(sp)	# Zero out old fp for debugger
	move	s0, a0				# save argc
	move	s1, a1				# save argv

	la	a0, __start+512			# Merge together from
	la	a1, __start+1024		# disklabel split.
	jal	memcpy
	li	a2, 8192 - 1024

	la	a0, edata			# clear BSS
	li	a1, 0
	la	a2, end
	jal	memset				# memset(edata, 0, end - edata)
	subu	a2, a2, a0

	move	a0, s0				# restore argc
	jal	main				# main(argc, argv)
	move	a1, s1				# restore argv
/* MAIN NEVER RETURNS */


/* dummy routines return 0 */
	.globl	getenv
	.globl	__main
getenv:
__main:
	.set	noreorder
	j	ra
	li	v0, 0

	.globl	strcmp
strcmp:
	.set	noreorder
1:
	lbu	v0, 0(a0)
	lbu	v1, 0(a1)
	beqz	v0, 2f
	addu	a0, a0, 1
	beq	v0, v1, 1b
	addu	a1, a1, 1
2:
	jr	ra
	subu	v0, v0, v1

	.globl	strlen
strlen:
	.set	noreorder
	li	v0, 0
1:
	lbu	v1, 0(a0)
	addu	a0, a0, 1
	bnez	v1, 1b
	addu	v0, v0, 1

	jr	ra
	addu	v0, v0, -1

	.globl	memset
memset:
	.set	noreorder
	move	v0, a0
1:
	beqz	a2, 2f
	addu	a2, a2, -1

	sb	a1, 0(a0)
	b	1b
	addu	a0, a0, 1
2:
	jr	ra
	nop

	.globl	memcpy
memcpy:
	.set	noreorder
	beqz	a2, 3f
	move	v0, a0
	sltu	v1, a1, a0
	beqz	v1, 1f
	li	a3, 1		/* Forward copy */

	li	a3, -1		/* Backwrads copy */
	addu	a0, a2
	b	2f
	addu	a1, a2

1:
	lbu	v1, 0(a1)
	addu	a2, a2, -1
	sb	v1, 0(a0)
2:
	addu	a0, a0, a3
	bnez	a2, 1b
	addu	a1, a1, a3

3:
	jr	ra
	nop

	.globl	twiddle
twiddle:
	.set	noreorder
	la	v0, twc+4
	lw	a0, -4(v0)
	li	a1, 7
	addu	a0, a0, 2
	sw	a0, -4(v0)
	and	a1, a0, a1
	add	a1, a1, v0

	li	a0, 1		/* Stdout */
	b	prom_write
	li	a2, 2

/*
 * Boot rom entrypoints.
 */

#define BOOTVEC 0xbfc00500	/* Address of boot vector table */

	.globl	getchar
getchar:
	.set	noreorder
	sw	ra, 4(sp)

	move	a1, sp
	li	a0, 0
	jal	prom_read
	li	a2, 1

	lw	ra, 4(sp)
	lbu	v0, 0(sp)
	jr	ra
	nop

	.globl	disk_open
disk_open:
	.set	noreorder
	b	__go
	li	v0, 96

	.globl	disk_read
disk_read:
	.set	noreorder
	b	__go
	li	v0, 100

	.globl	prom_read
prom_read:
	.set	noreorder
	b	__go
	li	v0, 0

	.globl	putchar
putchar:
	.set	noreorder
	sb	a0, 0(sp)
	move	a1, sp
	li	a0, 1		/* Stdout */
	li	a2, 1
/* -- fall through -- */
	.globl	prom_write
prom_write:
	.set	noreorder
	b	__go
	li	v0, 4

	.globl	printf
printf:
	.set	noreorder
	b	__go
	li	v0, 20

	.globl	panic
panic:

__go:
	.set	noreorder
	lw	v0, BOOTVEC(v0)
	nop
	jr	v0
	nop

	.align	2
	.globl	twc
twc:	.word	0
tws:	.ascii	"\\\b-\b/\b-\b"

