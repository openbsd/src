/* $NetBSD: div.S,v 1.1 1996/02/16 20:48:16 mark Exp $ */

/*
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
 *	This product includes software developed by the RiscBSD kernel team
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	$Id: div.S,v 1.1 1996/05/01 12:51:17 deraadt Exp $
 */

a1	.req	r0
a2	.req	r1
a3	.req	r2
a4	.req	r3
v1	.req	r4
v2	.req	r5
v3	.req	r6
v4	.req	r7
v5	.req	r8
v6	.req	r9
v7	.req	r10
fp	.req	r11
ip	.req	r12
sp	.req	r13
lr	.req	r14
pc	.req	r15

.text

	.global	__rt_sdiv
__rt_sdiv:
	B	x_divide

	.global	__rt_udiv
__rt_udiv:
	B	x_udivide

	.global	___umodsi3
___umodsi3:
	MOV	a3, a1
	MOV	a1, a2
	MOV	a2, a3
	B	x_uremainder

	.global	___udivsi3
___udivsi3:
	MOV	a3, a1
	MOV	a1, a2
	MOV	a2, a3
	B	x_udivide

	.global	___modsi3
___modsi3:
	MOV	a3, a1
	MOV	a1, a2
	MOV	a2, a3
	B	x_remainder

	.global	___divsi3
___divsi3:
	MOV	a3, a1
	MOV	a1, a2
	MOV	a2, a3
	B	x_divide

	.global	x_divtest
x_divtest:
	MOV	pc,lr

	.global	x_remainder
x_remainder:
	STMFD	sp!,{lr}
	BL	x_divide
	MOV	a1,a2
	LDMFD	sp!,{pc}

	.global	x_uremainder
x_uremainder:
	STMFD	sp!,{lr}
	BL	x_udivide
	MOV	a1,a2
	LDMFD	sp!,{pc}

x_overflow:
	MVN	a1,#0
	MOV	pc,lr

	.global	x_udivide		/* a1 = a2 / a1; a2 = a2 % a1 */
x_udivide:
	CMP	a1,#1
	BCC	x_overflow
	BEQ	x_divide_l0
	MOV	ip,#0
	MOVS	a2,a2
	BPL	x_divide_l1
	ORR	ip,ip,#0x20000000	/* ip bit 0x20000000 = -ve a2 */
	MOVS	a2,a2,lsr #1
	ORRCS	ip,ip,#0x10000000	/* ip bit 0x10000000 = bit 0 of a2 */
	B	x_divide_l1

x_divide_l0:				/* a1 == 1 */
	MOV	a1,a2
	MOV	a2,#0
	MOV	pc,lr

	.global	x_divide		/* a1 = a2 / a1; a2 = a2 % a1 */
x_divide:
	CMP	a1,#1
	BCC	x_overflow
	BEQ	x_divide_l0
	ANDS	ip,a1,#0x80000000
	RSBMI	a1,a1,#0
	ANDS	a3,a2,#0x80000000
	EOR	ip,ip,a3
	RSBMI	a2,a2,#0
	ORR	ip,a3,ip,lsr #1	/* ip bit 0x40000000 = -ve division */
				/* ip bit 0x80000000 = -ve remainder */

x_divide_l1:
	MOV	a3,#1
	MOV	a4,#0

	CMP	a2,a1
	BCC	x_divide_b0
	CMP	a2,a1,lsl #1
	BCC	x_divide_b1
	CMP	a2,a1,lsl #2
	BCC	x_divide_b2
	CMP	a2,a1,lsl #3
	BCC	x_divide_b3
	CMP	a2,a1,lsl #4
	BCC	x_divide_b4
	CMP	a2,a1,lsl #5
	BCC	x_divide_b5
	CMP	a2,a1,lsl #6
	BCC	x_divide_b6
	CMP	a2,a1,lsl #7
	BCC	x_divide_b7
	CMP	a2,a1,lsl #8
	BCC	x_divide_b8
	CMP	a2,a1,lsl #9
	BCC	x_divide_b9
	CMP	a2,a1,lsl #10
	BCC	x_divide_b10
	CMP	a2,a1,lsl #11
	BCC	x_divide_b11
	CMP	a2,a1,lsl #12
	BCC	x_divide_b12
	CMP	a2,a1,lsl #13
	BCC	x_divide_b13
	CMP	a2,a1,lsl #14
	BCC	x_divide_b14
	CMP	a2,a1,lsl #15
	BCC	x_divide_b15
	CMP	a2,a1,lsl #16
	BCC	x_divide_b16
	CMP	a2,a1,lsl #17
	BCC	x_divide_b17
	CMP	a2,a1,lsl #18
	BCC	x_divide_b18
	CMP	a2,a1,lsl #19
	BCC	x_divide_b19
	CMP	a2,a1,lsl #20
	BCC	x_divide_b20
	CMP	a2,a1,lsl #21
	BCC	x_divide_b21
	CMP	a2,a1,lsl #22
	BCC	x_divide_b22
	CMP	a2,a1,lsl #23
	BCC	x_divide_b23
	CMP	a2,a1,lsl #24
	BCC	x_divide_b24
	CMP	a2,a1,lsl #25
	BCC	x_divide_b25
	CMP	a2,a1,lsl #26
	BCC	x_divide_b26
	CMP	a2,a1,lsl #27
	BCC	x_divide_b27
	CMP	a2,a1,lsl #28
	BCC	x_divide_b28
	CMP	a2,a1,lsl #29
	BCC	x_divide_b29
	CMP	a2,a1,lsl #30
	BCC	x_divide_b30
	CMP	a2,a1,lsl #31
	SUBHS	a2,a2,a1,lsl #31
	ADDHS	a4,a4,a3,lsl #31
	CMP	a2,a1,lsl #30
	SUBHS	a2,a2,a1,lsl #30
	ADDHS	a4,a4,a3,lsl #30
x_divide_b30:
	CMP	a2,a1,lsl #29
	SUBHS	a2,a2,a1,lsl #29
	ADDHS	a4,a4,a3,lsl #29
x_divide_b29:
	CMP	a2,a1,lsl #28
	SUBHS	a2,a2,a1,lsl #28
	ADDHS	a4,a4,a3,lsl #28
x_divide_b28:
	CMP	a2,a1,lsl #27
	SUBHSS	a2,a2,a1,lsl #27
	ADDHS	a4,a4,a3,lsl #27
x_divide_b27:
	CMP	a2,a1,lsl #26
	SUBHS	a2,a2,a1,lsl #26
	ADDHS	a4,a4,a3,lsl #26
x_divide_b26:
	CMP	a2,a1,lsl #25
	SUBHS	a2,a2,a1,lsl #25
	ADDHS	a4,a4,a3,lsl #25
x_divide_b25:
	CMP	a2,a1,lsl #24
	SUBHS	a2,a2,a1,lsl #24
	ADDHS	a4,a4,a3,lsl #24
x_divide_b24:
	CMP	a2,a1,lsl #23
	SUBHS	a2,a2,a1,lsl #23
	ADDHS	a4,a4,a3,lsl #23
x_divide_b23:
	CMP	a2,a1,lsl #22
	SUBHS	a2,a2,a1,lsl #22
	ADDHS	a4,a4,a3,lsl #22
x_divide_b22:
	CMP	a2,a1,lsl #21
	SUBHS	a2,a2,a1,lsl #21
	ADDHS	a4,a4,a3,lsl #21
x_divide_b21:
	CMP	a2,a1,lsl #20
	SUBHS	a2,a2,a1,lsl #20
	ADDHS	a4,a4,a3,lsl #20
x_divide_b20:
	CMP	a2,a1,lsl #19
	SUBHS	a2,a2,a1,lsl #19
	ADDHS	a4,a4,a3,lsl #19
x_divide_b19:
	CMP	a2,a1,lsl #18
	SUBHS	a2,a2,a1,lsl #18
	ADDHS	a4,a4,a3,lsl #18
x_divide_b18:
	CMP	a2,a1,lsl #17
	SUBHS	a2,a2,a1,lsl #17
	ADDHS	a4,a4,a3,lsl #17
x_divide_b17:
	CMP	a2,a1,lsl #16
	SUBHS	a2,a2,a1,lsl #16
	ADDHS	a4,a4,a3,lsl #16
x_divide_b16:
	CMP	a2,a1,lsl #15
	SUBHS	a2,a2,a1,lsl #15
	ADDHS	a4,a4,a3,lsl #15
x_divide_b15:
	CMP	a2,a1,lsl #14
	SUBHS	a2,a2,a1,lsl #14
	ADDHS	a4,a4,a3,lsl #14
x_divide_b14:
	CMP	a2,a1,lsl #13
	SUBHS	a2,a2,a1,lsl #13
	ADDHS	a4,a4,a3,lsl #13
x_divide_b13:
	CMP	a2,a1,lsl #12
	SUBHS	a2,a2,a1,lsl #12
	ADDHS	a4,a4,a3,lsl #12
x_divide_b12:
	CMP	a2,a1,lsl #11
	SUBHS	a2,a2,a1,lsl #11
	ADDHS	a4,a4,a3,lsl #11
x_divide_b11:
	CMP	a2,a1,lsl #10
	SUBHS	a2,a2,a1,lsl #10
	ADDHS	a4,a4,a3,lsl #10
x_divide_b10:
	CMP	a2,a1,lsl #9
	SUBHS	a2,a2,a1,lsl #9
	ADDHS	a4,a4,a3,lsl #9
x_divide_b9:
	CMP	a2,a1,lsl #8
	SUBHS	a2,a2,a1,lsl #8
	ADDHS	a4,a4,a3,lsl #8
x_divide_b8:
	CMP	a2,a1,lsl #7
	SUBHS	a2,a2,a1,lsl #7
	ADDHS	a4,a4,a3,lsl #7
x_divide_b7:
	CMP	a2,a1,lsl #6
	SUBHS	a2,a2,a1,lsl #6
	ADDHS	a4,a4,a3,lsl #6
x_divide_b6:
	CMP	a2,a1,lsl #5
	SUBHS	a2,a2,a1,lsl #5
	ADDHS	a4,a4,a3,lsl #5
x_divide_b5:
	CMP	a2,a1,lsl #4
	SUBHS	a2,a2,a1,lsl #4
	ADDHS	a4,a4,a3,lsl #4
x_divide_b4:
	CMP	a2,a1,lsl #3
	SUBHS	a2,a2,a1,lsl #3
	ADDHS	a4,a4,a3,lsl #3
x_divide_b3:
	CMP	a2,a1,lsl #2
	SUBHS	a2,a2,a1,lsl #2
	ADDHS	a4,a4,a3,lsl #2
x_divide_b2:
	CMP	a2,a1,lsl #1
	SUBHS	a2,a2,a1,lsl #1
	ADDHS	a4,a4,a3,lsl #1
x_divide_b1:
	CMP	a2,a1
	SUBHS	a2,a2,a1
	ADDHS	a4,a4,a3
x_divide_b0:

	TST	ip,#0x20000000
	BNE	x_udivide_l1
	MOV	a1,a4
	CMP	ip,#0
	RSBMI	a2,a2,#0
	MOVS	ip,ip,lsl #1
	RSBMI	a1,a1,#0
	MOV	pc,lr

x_udivide_l1:
	TST	ip,#0x10000000
	MOV	a2,a2,lsl #1
	ORRNE	a2,a2,#1
	MOV	a4,a4,lsl #1
	CMP	a2,a1
	SUBHS	a2,a2,a1
	ADDHS	a4,a4,a3
	MOV	a1,a4
	MOV	pc,lr

