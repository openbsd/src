/* $NetBSD: fpemul.S,v 1.2 1996/03/18 19:58:16 mark Exp $ */

/*
 * Copyright (c) 1995 Mark Brinicombe.
 * Copyright (c) 1995 Neil Carson.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the RiscBSD kernel team.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
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
 * fpemul.S
 *
 * FP multiply
 *
 * Created      : 11/02/95
 */

.global _nc_fp_mul

pc	.req	r15

mant1	.req	r2
xp1	.req	r3
mant2	.req	r1
xp2	.req	r4
wrk1	.req	r14
reshigh	.req	r4

_nc_fp_mul:
    stmfd r13!,{r2-r4,r14}
    /* check for null result - bloody lucky but you never know... */
    cmp r0,#0
    cmpne r1,#0
    moveq r0,#0
    beq fp_mul_exit

    mov xp1,r0,lsr #23             		/* get exponent+sign */
    bic mant1,r0,xp1,lsl #23       		/* get mantissa */
    orr mant1,mant1,#1<<23          		/* set on bit to left of point */
    and xp1,xp1,#255              		/* remove sign */

    eor r0,r0,r1                    		/* sign of result */
    and r0,r0,#1<<31                		/* clear rest of result */

    mov xp2,r1,lsr #23             		/* get exponent+sign */
    bic mant2,r1,xp2,lsl #23       		/* get mantissa */
    orr mant2,mant2,#1<<23          		/* set on bit to left of point */
    and xp2,xp2,#255              		/* remove sign */

    add xp1,xp1,xp2
    subs xp1,xp1,#127             		/* exponent result */
    movmi r0,#0                     		/* underflow */
    bmi fp_mul_exit
    /* do the multiply to 30 bits precision */
    mov wrk1,mant1,lsr#9            		/* top 15 bits */
    mov reshigh,mant2,lsr#9         		/* top 15 bits */
    bic mant1,mant1,wrk1,lsl#9      		/* low  9 bits */
    bic mant2,mant2,reshigh,lsl#9   		/* low  9 bits */
    mul mant2,wrk1,mant2            		/* top*low */
    mul mant1,reshigh,mant1         		/* low*top */
    mul reshigh,wrk1,reshigh        		/* top*top */
    add reshigh,reshigh,mant1,lsr#9 		/* top + middle bit1 */
    add reshigh,reshigh,mant2,lsr#9 		/* top + middle bit2 */
    /* adjust for carry */
    tst reshigh,#1<<29
    addne xp1,xp1,#1              		/* adjust exponent */
    movne reshigh,reshigh,lsr#1     		/* shift result right */
    /* combine result */
    orr r0,r0,xp1,lsl #23          		/* set exponent */
    bic reshigh,reshigh,#1<<28      		/* clear hi-order */
    orr r0,r0,reshigh,lsr #5        		/* top 23 bits */
fp_mul_exit:
    ldmfd r13!,{r2-r4,pc}
