/* $NetBSD: fpediv.S,v 1.2 1996/03/18 19:58:14 mark Exp $ */

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
 * fpediv.S
 *
 * FP divide
 *
 * Created      : 11/02/95
 */

.global _nc_fp_div

pc	.req	r15

mant1	.req	r2
xp1	.req	r3
mant2	.req	r4
xp2	.req	r14
q	.req	r1
p	.req	r14

_nc_fp_div:
    stmfd r13!,{r2-r4,r14}
    /* check for null result */
    cmp r0,#0
    cmpne r1,#0
    moveq r0,#0
    beq fp_div_exit

    mov xp1,r0,lsr #23             		/* get exponent+sign */
    bic mant1,r0,xp1,lsl #23       		/* get mantissa */
    orr mant1,mant1,#1<<23          		/* set on bit to left of point */
    and xp1,xp1,#255              		/* remove sign */

    mov xp2,r1,lsr #23             		/* get exponent+sign */
    bic mant2,r1,xp2,lsl #23       		/* get mantissa */
    orr mant2,mant2,#1<<23          		/* set on bit to left of point */
    and xp2,xp2,#255              		/* remove sign */

    sub xp1,xp1,xp2
    adds xp1,xp1,#127             		/* exponent result */
    movmi r0,#0                     		/* underflow */
    bmi fp_div_exit
    eor r0,r0,r1                    		/* sign of result */
    and r0,r0,#1<<31                		/* clear rest of result */
    /* do the divide to full precision */
    mov q,#0                        		/* quotient */
    mov p,#1<<23                    		/* number binary places to do */
fp_divloop:                     		/* do binary places */
    cmp mant1,mant2                 		/* number>divisor? */
    orrcs q,q,p                     		/* yes - set bit in binary place */
    subcs mant1,mant1,mant2         		/* discard accounted value */
    movs mant1,mant1,lsl #1         		/* shift up remainder, if any */
    movnes p,p,lsr#1                		/* next binary place, if remainder */
    bne fp_divloop                  		/* next place if any */

    cmp q,#1<<23                    		/* normalise? */
    sublt xp1,xp1,#1              		/* yes, adjust exponent */
    movlt q,q,lsl #1                		/* & mantissa */
    /* combine result */
    orr r0,r0,xp1,lsl #23          		/* set exponent */
    bic q,q,#1<<23                  		/* clear hi-order */
    orr r0,r0,q                     		/* result */
fp_div_exit:
    ldmfd r13!,{r2-r4,pc}

