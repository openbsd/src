/* $arla: process.mips.S,v 1.1 2000/01/02 02:11:17 lha Exp $ */

/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

#include <config.h>

#undef RCSID

#ifdef HAVE_PIC
	.option	pic2

#if defined(HAVE_MACHINE_REGDEF_H) || defined(__OpenBSD__)
#include <machine/regdef.h>
#elif defined(HAVE_REGDEF_H)
#include <regdef.h>
#endif
	
/* 9 sregs, ra, 6 fp regs, gp, pad to 8 byte boundary */
#define regspace 9 * 4 + 4 + 6 * 8 + 4 + 4
#define floats 0
#define registers floats + 6 * 8
#define returnaddr regspace - 4
#define topstack 0
#define GPOFF	regspace - 8
	.globl savecontext /* MIPS' C compiler doesnt prepend underscores. */
	.ent savecontext /* Insert debugger information. */
savecontext:
        .set    noreorder
        .cpload t9                      # set up gp for KPIC
        .set    reorder
        subu sp, regspace
        .cprestore GPOFF                # trigger t9/jalr
	.set	noreorder
	li	t0, 1
	.extern	PRE_Block
	sb	t0, PRE_Block
        .set    reorder
	.frame	sp, regspace, ra
/* Save registers. */
	sw	s0, registers + 0(sp)
	sw	s1, registers + 4(sp)
	sw	s2, registers + 8(sp)
	sw	s3, registers + 12(sp)
	sw	s4, registers + 16(sp)
	sw	s5, registers + 20(sp)
	sw	s6, registers + 24(sp)
	sw	s7, registers + 28(sp)
	sw	s8, registers + 32(sp)
/* Save return address */
	sw	ra, returnaddr(sp)
	.mask	0xc0ff0000, -4
/* Need to save floating point registers? */
	s.d	$f20, floats + 0(sp)
	s.d	$f22, floats + 8(sp)
	s.d	$f24, floats + 16(sp)
	s.d	$f26, floats + 24(sp)
	s.d	$f28, floats + 32(sp)
	s.d	$f30, floats + 40(sp)
	.fmask	0x55400000, regspace
	sw	sp, topstack(a1)
	beq	a2, $0, samestack
	move	sp, a2
samestack:
	move	t9, a0
	j	t9
	.end	savecontext

	.globl	returnto
	.ent	returnto
returnto:
        .set    noreorder
        .cpload t9                      # set up gp for KPIC
        .set    reorder

	lw	sp, topstack(a0)
	lw	s0, registers + 0(sp)
	lw	s1, registers + 4(sp)
	lw	s2, registers + 8(sp)
	lw	s3, registers + 12(sp)
	lw	s4, registers + 16(sp)
	lw	s5, registers + 20(sp)
	lw	s6, registers + 24(sp)
	lw	s7, registers + 28(sp)
	lw	s8, registers + 32(sp)
/* Save return address */
	lw	ra, returnaddr(sp)
/* Need to save floating point registers? */
	l.d	$f20, floats + 0(sp)
	l.d	$f22, floats + 8(sp)
	l.d	$f24, floats + 16(sp)
	l.d	$f26, floats + 24(sp)
	l.d	$f28, floats + 32(sp)
	l.d	$f30, floats + 40(sp)
	.set	noreorder
	addu	sp, regspace
	la	t0, PRE_Block
	j	ra
	sb	zero, 0(t0)
	.set	reorder
	.end	returnto

#else
/* Code for MIPS R2000/R3000 architecture
 * Written by Zalman Stern April 30th, 1989.
 */

#if defined(HAVE_REGDEF_H)	
#include <regdef.h> /* Allow use of symbolic names for registers. */
#else
#define sp     $29
#define ra     $31
#define t0     $8
#define a0     $4
#define a1     $5
#define a2     $6
#define s0     $16
#define s1     $17
#define s2     $18
#define s3     $19
#define s4     $20
#define s5     $21
#define s6     $22
#define s7     $23
#define s8     $30
#endif
	
#define regspace 9 * 4 + 4 + 6 * 8
#define floats 0
#define registers floats + 6 * 8
#define returnaddr regspace - 4
#define topstack 0
	.globl savecontext /* MIPS' C compiler doesnt prepend underscores. */
	.ent savecontext /* Insert debugger information. */
savecontext:
	li	t0, 1
	.extern	PRE_Block
	sb	t0, PRE_Block
	subu	sp, regspace
	.frame	sp, regspace, ra
/* Save registers. */
	sw	s0, registers + 0(sp)
	sw	s1, registers + 4(sp)
	sw	s2, registers + 8(sp)
	sw	s3, registers + 12(sp)
	sw	s4, registers + 16(sp)
	sw	s5, registers + 20(sp)
	sw	s6, registers + 24(sp)
	sw	s7, registers + 28(sp)
	sw	s8, registers + 32(sp)
/* Save return address */
	sw	ra, returnaddr(sp)
	.mask	0xc0ff0000, -4
/* Need to save floating point registers? */
	s.d	$f20, floats + 0(sp)
	s.d	$f22, floats + 8(sp)
	s.d	$f24, floats + 16(sp)
	s.d	$f26, floats + 24(sp)
	s.d	$f28, floats + 32(sp)
	s.d	$f30, floats + 40(sp)
	.fmask	0x55400000, regspace
	sw	sp, topstack(a1)
	beq	a2, $0, samestack
	addu	sp, $0, a2
samestack:
	jal	a0
	.end	savecontext

	.globl	returnto
	.ent	returnto
returnto:
	lw	sp, topstack(a0)
	lw	s0, registers + 0(sp)
	lw	s1, registers + 4(sp)
	lw	s2, registers + 8(sp)
	lw	s3, registers + 12(sp)
	lw	s4, registers + 16(sp)
	lw	s5, registers + 20(sp)
	lw	s6, registers + 24(sp)
	lw	s7, registers + 28(sp)
	lw	s8, registers + 32(sp)
/* Save return address */
	lw	ra, returnaddr(sp)
/* Need to save floating point registers? */
	l.d	$f20, floats + 0(sp)
	l.d	$f22, floats + 8(sp)
	l.d	$f24, floats + 16(sp)
	l.d	$f26, floats + 24(sp)
	l.d	$f28, floats + 32(sp)
	l.d	$f30, floats + 40(sp)
	addu	sp, regspace
	sb	$0, PRE_Block
	j	ra
	.end	returnto
#endif	/* HAVE_PIC */
