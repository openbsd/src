/*	$OpenBSD: start.S,v 1.1 1997/05/11 16:17:54 pefo Exp $ */

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

	la	a0, edata			# clear BSS
	li	a1, 0
	la	a2, end
	jal	memset				# memset(edata, 0, end - edata)
	subu	a2, a2, a0

	move	a0, s0				# restore argc
	jal	main				# main(argc, argv)
	move	a1, s1				# restore argv

	j	restart				# restart...
	nop

/* dummy routine for gcc2 */
LEAF(__main)
	j	ra
	nop
END(__main)

/*
 * Boot rom entrypoints.
 */

#define BOOTVEC 0xbfc00500	/* Address of boot vector table */

NON_LEAF(getchar, STAND_FRAME_SIZE, ra)
	.mask	0x80000000, (STAND_RA_OFFSET - STAND_FRAME_SIZE)
	subu	sp, sp, STAND_FRAME_SIZE
	sw	ra, STAND_RA_OFFSET(sp)

	move	a1, sp
	li	a0, 0
	li	a2, 1
	jal	prom_read

	lbu	v0, 0(sp)
	lw	ra, STAND_RA_OFFSET(sp)
	addu	sp, sp, STAND_FRAME_SIZE
	jr	ra
END(getchar)

LEAF(putchar)
	sb	a0, 0(sp)
	move	a1, sp
	li	a0, 1		/* Stdout */
	li	a2, 1
	b	prom_write
	
END(putchar)

LEAF(restart)
END(restart)

LEAF(panic)
END(panic)

LEAF(disk_open)
	lw	v0, BOOTVEC+96
	jr	v0
END(disk_open)

LEAF(disk_read)
	lw	v0, BOOTVEC+100
	jr	v0
END(disk_read)


LEAF(prom_read)
	lw	v0, BOOTVEC+0
	jr	v0
END(prom_read)

LEAF(prom_write)
	lw	v0, BOOTVEC+4
	jr	v0
END(prom_write)
