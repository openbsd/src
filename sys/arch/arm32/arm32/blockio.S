/* $OpenBSD: blockio.S,v 1.2 2000/03/03 00:54:46 todd Exp $ */
/* $NetBSD: blockio.S,v 1.1 1996/01/31 23:15:22 mark Exp $ */

/*
 * Copyright (c) 1994 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * blockio.S
 *
 * optimised block read/write from/to IO routines.
 *
 * Created      : 08/10/94
 * Last updated : 12/05/95
 *
 */

sp	.req	r13
lr	.req	r14
pc	.req	r15

.text

	.global	_insw
/*
 * Reads short ints (16 bits) from an I/O address into a block of memory
 *
 * r0 = address to read from (IO)
 * r1 = address to write to (memory)
 * r2 = length
 */

_insw:
/* Make sure that we have a positive length */
	cmp	r2, #0x00000000
	movle	pc, lr

/* If the destination address and the size is word aligned, do it fast */

	tst	r2, #0x00000001
	tsteq	r1, #0x00000003
	beq	fastinsw

/* Non aligned insw */

inswloop:
	ldr	r3, [r0]
	strb	r3, [r1], #0x0001
	mov	r3, r3, lsr #8
	strb	r3, [r1], #0x0001
	subs	r2, r2, #0x00000001
	bgt	inswloop

	mov	pc, lr

/* Word aligned insw */

fastinsw:
	stmfd	r13!, {r4}

fastinswloop:
	ldr	r3, [r0, #0x0002]	/* take advantage of nonaligned
					 * word accesses */
	ldr	r4, [r0]
	mov	r3, r3, lsr #16		/* Put the two shorts together */
	orr	r3, r3, r4, lsl #16
	str	r3, [r1], #0x0004	/* Store */
	subs	r2, r2, #0x00000002	/* Next */
	bgt	fastinswloop

	ldmfd	r13!, {r4}

	mov	pc, lr


	.global	_outsw

/*
 * Writes short ints (16 bits) from a block of memory to an I/O address
 *
 * r0 = address to write to (IO)
 * r1 = address to read from (memory)
 * r2 = length
 */

_outsw:
/* Make sure that we have a positive length */
	cmp	r2, #0x00000000
	movle	pc, lr

#ifdef notyet

/*
 * The optimised routines have not been tested yet and I don't feel
 * like testing the new write code on 2.8 Gig of valuable data.
 * Need to wait until I hang another HD on my machine
 */

/* If the destination address and the size is word aligned, do it fast */

	tst	r2, #0x00000001
	tsteq	r1, #0x00000003
	beq	fastoutsw
#endif

/* Non aligned outsw */

	stmfd	sp!, {r4}

outswloop:
	ldrb	r3, [r1], #0x0001
	ldrb	r4, [r1], #0x0001
	orr	r3, r3, r4, lsl #8
	orr	r3, r3, r3, lsl #16
	str	r3, [r0]
	subs	r2, r2, #0x00000001
	bgt	outswloop

	ldmfd	sp!, {r4}

	mov	pc, lr

#ifdef notyet
/* Word aligned outsw */

fastoutsw:
	stmfd	r13!, {r4}

fastoutswloop:
	ldr	r3, [r1], #0x0004

	mov	r4, r3, lsl #16
	orr	r4, r4, lsr #16
	str	r4, [r0]

	mov	r4, r3, lsr #16
	orr	r4, r4, lsl #16
	str	r4, [r0]

	subs	r2, r2, #0x00000002
	bgt	outswloop

	ldmfd	sp!, {r4}

	mov	pc, lr
#endif

	.global	_insw16
/*
 * reads short ints (16 bits) from an I/O address into a block of memory
 * with a length garenteed to be a multiple of 16 bytes
 * with a word aligned destination address
 *
 * r0 = address to read from (IO)
 * r1 = address to write to (memory)
 * r2 = length
 */

_insw16:
/* Make sure that we have a positive length */
	cmp	r2, #0x00000000
	movle	pc, lr

/* If the destination address and the size is word aligned, do it fast */

	tst	r2, #0x0000000f
	tsteq	r1, #0x00000003

	bne	_insw

/* Word aligned insw */

	stmfd	r13!, {r4-r7}

insw16loop:
	ldr	r3, [r0, #0x0002]	/* take advantage of nonaligned
					 * word accesses */
	ldr	r7, [r0]
	mov	r3, r3, lsr #16		/* Put the two shorts together */
	orr	r3, r3, r7, lsl #16

	ldr	r4, [r0, #0x0002]	/* take advantage of nonaligned
					 * word accesses */
	ldr	r7, [r0]
	mov	r4, r4, lsr #16		/* Put the two shorts together */
	orr	r4, r4, r7, lsl #16

	ldr	r5, [r0, #0x0002]	/* take advantage of nonaligned
					 * word accesses */
	ldr	r7, [r0]
	mov	r5, r5, lsr #16		/* Put the two shorts together */
	orr	r5, r5, r7, lsl #16

	ldr	r6, [r0, #0x0002]	/* take advantage of nonaligned
					 * word accesses */
	ldr	r7, [r0]
	mov	r6, r6, lsr #16		/* Put the two shorts together */
	orr	r6, r6, r7, lsl #16

	stmia	r1!, {r3-r6}
	subs	r2, r2, #0x00000008	/* Next */
	bgt	insw16loop

	ldmfd	r13!, {r4-r7}

	mov	pc, lr


#ifdef notyet
/*
 * The optimised routines have not been tested yet and I don't feel
 * like testing the new write code on 2.8 Gig of valuable data.
 * Need to wait until I hang another HD on my machine
 */
	.global	_outsw16
/*
 * Writes short ints (16 bits) from a block of memory to an I/O address
 *
 * r0 = address to write to (IO)
 * r1 = address to read from (memory)
 * r2 = length
 */

_outsw16:
/* Make sure that we have a positive length */
	cmp	r2, #0x00000000
	movle	pc, lr

/* If the destination address and the size is word aligned, do it fast */

	tst	r2, #0x0000000f
	tsteq	r1, #0x00000003

	bne	_outsw

/* Word aligned outsw */

	stmfd	r13!, {r4-r8}

outsw16loop:
	ldmia	r1!, {r4-r8}

	mov	r3, r4, lsl #16
	orr	r3, r3, lsr #16
	str	r3, [r0]

	mov	r3, r4, lsr #16
	orr	r3, r4, lsl #16
	str	r3, [r0]

	mov	r3, r5, lsl #16
	orr	r3, r3, lsr #16
	str	r3, [r0]

	mov	r3, r5, lsr #16
	orr	r3, r4, lsl #16
	str	r3, [r0]

	mov	r3, r6, lsl #16
	orr	r3, r3, lsr #16
	str	r3, [r0]

	mov	r3, r6, lsr #16
	orr	r3, r4, lsl #16
	str	r3, [r0]

	mov	r3, r7, lsl #16
	orr	r3, r3, lsr #16
	str	r3, [r0]

	mov	r3, r7, lsr #16
	orr	r3, r4, lsl #16
	str	r3, [r0]

	subs	r2, r2, #0x00000008
	bgt	outsw16loop

	ldmfd	sp!, {r4-r8}

	mov	pc, lr
#endif
