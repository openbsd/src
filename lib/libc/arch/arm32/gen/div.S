/* $NetBSD: div.S,v 1.2 1996/05/12 20:52:09 mark Exp $ */

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
 */

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
	MOV	r2, r0
	MOV	r0, r1
	MOV	r1, r2
	B	x_uremainder

	.global	___udivsi3
___udivsi3:
	MOV	r2, r0
	MOV	r0, r1
	MOV	r1, r2
	B	x_udivide

	.global	___modsi3
___modsi3:
	MOV	r2, r0
	MOV	r0, r1
	MOV	r1, r2
	B	x_remainder

	.global	___divsi3
___divsi3:
	MOV	r2, r0
	MOV	r0, r1
	MOV	r1, r2
	B	x_divide

	.global	x_divtest
x_divtest:
	MOV	pc,lr

	.global	x_remainder
x_remainder:
	STMFD	sp!,{lr}
	BL	x_divide
	MOV	r0,r1
	LDMFD	sp!,{pc}

	.global	x_uremainder
x_uremainder:
	STMFD	sp!,{lr}
	BL	x_udivide
	MOV	r0,r1
	LDMFD	sp!,{pc}

x_overflow:
	MVN	r0,#0
	MOV	pc,lr

	.global	x_udivide		/* r0 = r1 / r0; r1 = r1 % r0 */
x_udivide:
	CMP	r0,#1
	BCC	x_overflow
	BEQ	x_divide_l0
	MOV	ip,#0
	MOVS	r1,r1
	BPL	x_divide_l1
	ORR	ip,ip,#0x20000000	/* ip bit 0x20000000 = -ve r1 */
	MOVS	r1,r1,lsr #1
	ORRCS	ip,ip,#0x10000000	/* ip bit 0x10000000 = bit 0 of r1 */
	B	x_divide_l1

x_divide_l0:				/* r0 == 1 */
	MOV	r0,r1
	MOV	r1,#0
	MOV	pc,lr

	.global	x_divide		/* r0 = r1 / r0; r1 = r1 % r0 */
x_divide:
	CMP	r0,#1
	BCC	x_overflow
	BEQ	x_divide_l0
	ANDS	ip,r0,#0x80000000
	RSBMI	r0,r0,#0
	ANDS	r2,r1,#0x80000000
	EOR	ip,ip,r2
	RSBMI	r1,r1,#0
	ORR	ip,r2,ip,lsr #1	/* ip bit 0x40000000 = -ve division */
				/* ip bit 0x80000000 = -ve remainder */

x_divide_l1:
	MOV	r2,#1
	MOV	r3,#0

	CMP	r1,r0
	BCC	x_divide_b0
	CMP	r1,r0,lsl #1
	BCC	x_divide_b1
	CMP	r1,r0,lsl #2
	BCC	x_divide_b2
	CMP	r1,r0,lsl #3
	BCC	x_divide_b3
	CMP	r1,r0,lsl #4
	BCC	x_divide_b4
	CMP	r1,r0,lsl #5
	BCC	x_divide_b5
	CMP	r1,r0,lsl #6
	BCC	x_divide_b6
	CMP	r1,r0,lsl #7
	BCC	x_divide_b7
	CMP	r1,r0,lsl #8
	BCC	x_divide_b8
	CMP	r1,r0,lsl #9
	BCC	x_divide_b9
	CMP	r1,r0,lsl #10
	BCC	x_divide_b10
	CMP	r1,r0,lsl #11
	BCC	x_divide_b11
	CMP	r1,r0,lsl #12
	BCC	x_divide_b12
	CMP	r1,r0,lsl #13
	BCC	x_divide_b13
	CMP	r1,r0,lsl #14
	BCC	x_divide_b14
	CMP	r1,r0,lsl #15
	BCC	x_divide_b15
	CMP	r1,r0,lsl #16
	BCC	x_divide_b16
	CMP	r1,r0,lsl #17
	BCC	x_divide_b17
	CMP	r1,r0,lsl #18
	BCC	x_divide_b18
	CMP	r1,r0,lsl #19
	BCC	x_divide_b19
	CMP	r1,r0,lsl #20
	BCC	x_divide_b20
	CMP	r1,r0,lsl #21
	BCC	x_divide_b21
	CMP	r1,r0,lsl #22
	BCC	x_divide_b22
	CMP	r1,r0,lsl #23
	BCC	x_divide_b23
	CMP	r1,r0,lsl #24
	BCC	x_divide_b24
	CMP	r1,r0,lsl #25
	BCC	x_divide_b25
	CMP	r1,r0,lsl #26
	BCC	x_divide_b26
	CMP	r1,r0,lsl #27
	BCC	x_divide_b27
	CMP	r1,r0,lsl #28
	BCC	x_divide_b28
	CMP	r1,r0,lsl #29
	BCC	x_divide_b29
	CMP	r1,r0,lsl #30
	BCC	x_divide_b30
	CMP	r1,r0,lsl #31
	SUBHS	r1,r1,r0,lsl #31
	ADDHS	r3,r3,r2,lsl #31
	CMP	r1,r0,lsl #30
	SUBHS	r1,r1,r0,lsl #30
	ADDHS	r3,r3,r2,lsl #30
x_divide_b30:
	CMP	r1,r0,lsl #29
	SUBHS	r1,r1,r0,lsl #29
	ADDHS	r3,r3,r2,lsl #29
x_divide_b29:
	CMP	r1,r0,lsl #28
	SUBHS	r1,r1,r0,lsl #28
	ADDHS	r3,r3,r2,lsl #28
x_divide_b28:
	CMP	r1,r0,lsl #27
	SUBHSS	r1,r1,r0,lsl #27
	ADDHS	r3,r3,r2,lsl #27
x_divide_b27:
	CMP	r1,r0,lsl #26
	SUBHS	r1,r1,r0,lsl #26
	ADDHS	r3,r3,r2,lsl #26
x_divide_b26:
	CMP	r1,r0,lsl #25
	SUBHS	r1,r1,r0,lsl #25
	ADDHS	r3,r3,r2,lsl #25
x_divide_b25:
	CMP	r1,r0,lsl #24
	SUBHS	r1,r1,r0,lsl #24
	ADDHS	r3,r3,r2,lsl #24
x_divide_b24:
	CMP	r1,r0,lsl #23
	SUBHS	r1,r1,r0,lsl #23
	ADDHS	r3,r3,r2,lsl #23
x_divide_b23:
	CMP	r1,r0,lsl #22
	SUBHS	r1,r1,r0,lsl #22
	ADDHS	r3,r3,r2,lsl #22
x_divide_b22:
	CMP	r1,r0,lsl #21
	SUBHS	r1,r1,r0,lsl #21
	ADDHS	r3,r3,r2,lsl #21
x_divide_b21:
	CMP	r1,r0,lsl #20
	SUBHS	r1,r1,r0,lsl #20
	ADDHS	r3,r3,r2,lsl #20
x_divide_b20:
	CMP	r1,r0,lsl #19
	SUBHS	r1,r1,r0,lsl #19
	ADDHS	r3,r3,r2,lsl #19
x_divide_b19:
	CMP	r1,r0,lsl #18
	SUBHS	r1,r1,r0,lsl #18
	ADDHS	r3,r3,r2,lsl #18
x_divide_b18:
	CMP	r1,r0,lsl #17
	SUBHS	r1,r1,r0,lsl #17
	ADDHS	r3,r3,r2,lsl #17
x_divide_b17:
	CMP	r1,r0,lsl #16
	SUBHS	r1,r1,r0,lsl #16
	ADDHS	r3,r3,r2,lsl #16
x_divide_b16:
	CMP	r1,r0,lsl #15
	SUBHS	r1,r1,r0,lsl #15
	ADDHS	r3,r3,r2,lsl #15
x_divide_b15:
	CMP	r1,r0,lsl #14
	SUBHS	r1,r1,r0,lsl #14
	ADDHS	r3,r3,r2,lsl #14
x_divide_b14:
	CMP	r1,r0,lsl #13
	SUBHS	r1,r1,r0,lsl #13
	ADDHS	r3,r3,r2,lsl #13
x_divide_b13:
	CMP	r1,r0,lsl #12
	SUBHS	r1,r1,r0,lsl #12
	ADDHS	r3,r3,r2,lsl #12
x_divide_b12:
	CMP	r1,r0,lsl #11
	SUBHS	r1,r1,r0,lsl #11
	ADDHS	r3,r3,r2,lsl #11
x_divide_b11:
	CMP	r1,r0,lsl #10
	SUBHS	r1,r1,r0,lsl #10
	ADDHS	r3,r3,r2,lsl #10
x_divide_b10:
	CMP	r1,r0,lsl #9
	SUBHS	r1,r1,r0,lsl #9
	ADDHS	r3,r3,r2,lsl #9
x_divide_b9:
	CMP	r1,r0,lsl #8
	SUBHS	r1,r1,r0,lsl #8
	ADDHS	r3,r3,r2,lsl #8
x_divide_b8:
	CMP	r1,r0,lsl #7
	SUBHS	r1,r1,r0,lsl #7
	ADDHS	r3,r3,r2,lsl #7
x_divide_b7:
	CMP	r1,r0,lsl #6
	SUBHS	r1,r1,r0,lsl #6
	ADDHS	r3,r3,r2,lsl #6
x_divide_b6:
	CMP	r1,r0,lsl #5
	SUBHS	r1,r1,r0,lsl #5
	ADDHS	r3,r3,r2,lsl #5
x_divide_b5:
	CMP	r1,r0,lsl #4
	SUBHS	r1,r1,r0,lsl #4
	ADDHS	r3,r3,r2,lsl #4
x_divide_b4:
	CMP	r1,r0,lsl #3
	SUBHS	r1,r1,r0,lsl #3
	ADDHS	r3,r3,r2,lsl #3
x_divide_b3:
	CMP	r1,r0,lsl #2
	SUBHS	r1,r1,r0,lsl #2
	ADDHS	r3,r3,r2,lsl #2
x_divide_b2:
	CMP	r1,r0,lsl #1
	SUBHS	r1,r1,r0,lsl #1
	ADDHS	r3,r3,r2,lsl #1
x_divide_b1:
	CMP	r1,r0
	SUBHS	r1,r1,r0
	ADDHS	r3,r3,r2
x_divide_b0:

	TST	ip,#0x20000000
	BNE	x_udivide_l1
	MOV	r0,r3
	CMP	ip,#0
	RSBMI	r1,r1,#0
	MOVS	ip,ip,lsl #1
	RSBMI	r0,r0,#0
	MOV	pc,lr

x_udivide_l1:
	TST	ip,#0x10000000
	MOV	r1,r1,lsl #1
	ORRNE	r1,r1,#1
	MOV	r3,r3,lsl #1
	CMP	r1,r0
	SUBHS	r1,r1,r0
	ADDHS	r3,r3,r2
	MOV	r0,r3
	MOV	pc,lr

