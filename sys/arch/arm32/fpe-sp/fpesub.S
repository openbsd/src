/* $NetBSD: fpesub.S,v 1.2 1996/03/18 19:58:18 mark Exp $ */

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
 * fpesub.S
 *
 * FP subtract
 *
 * Created      : 11/02/95
 */

.global _nc_fp_sub

pc	.req	r15

mant1	.req	r2
xp1	.req	r3
mant2	.req	r4
xp2	.req	r5
wrk1	.req	r14

_nc_fp_sub:
    stmfd r13!,{r2-r5,r14}
    mov xp1,r0,lsr #23             		/* get exponent+sign */
    bic mant1,r0,xp1,lsl #23       		/* get mantissa */
    cmp r0,#0
    orrne mant1,mant1,#1<<23        		/* set on bit to left of point */
    and xp1,xp1,#255              		/* remove sign */

    mov xp2,r1,lsr #23             		/* get exponent+sign */
    bic mant2,r1,xp2,lsl #23       		/* get mantissa */
    cmp r1,#0
    orrne mant2,mant2,#1<<23        		/* set on bit to left of point */
    and xp2,xp2,#255              		/* remove sign */

    eors wrk1,r0,r1                 		/* both +ve/-ve? */
    bmi fp_sub_unequal              		/* no - special case */

    subs wrk1,xp1,xp2             		/* exp. difference */
    subpl mant1,mant1,mant2,lsr wrk1      	/* exp1>exp2, round mant2 & subtract */
    rsbmi wrk1,wrk1,#0              		/* as +ve difference */
    rsbmi mant1,mant2,mant1,lsr wrk1      	/* exp1<exp2, round mant1 & subtract */
    movmi xp1,xp2                 		/* exponent of result */
    and r0,r0,#1<<31                		/* default sign of result */
    cmp mant1,#0                    		/* flip sign? */
    rsbmi mant1,mant1,#0            		/* yes - flip mantissa */
    eormi r0,r0,#1<<31              		/* and final sign */
    /* normalise result - adjust mantissa & exponent */
    cmp mant1,#0                    		/* check for 0 - special case */
    moveq r0,#0
    beq fp_sub_exit
fp_sub_roundup:
    tst mant1,#1<<23
    bne fp_sub_normexit
    sub xp1,xp1,#1
    mov mant1,mant1,lsl #1
    b fp_sub_roundup

    /* r0 & r1 have different signs - add */
fp_sub_unequal:
    subs wrk1,xp1,xp2             		/* exp. difference */
    addpl mant1,mant1,mant2,lsr wrk1      /* exp1>exp2, round mant2 & add */
    rsbmi wrk1,wrk1,#0              		/* as +ve difference */
    addmi mant1,mant2,mant1,lsr wrk1      /* exp1<exp2, round mant1 & add */
    movmi xp1,xp2                 		/* exponent of result */
    and r0,r0,#1<<31                		/* sign of result */
    /* normalise result - adjust mantissa & exponent */
fp_sub_rounddown:
    cmp mant1,#1<<24
    blt fp_sub_normexit
    add xp1,xp1,#1
    mov mant1,mant1,lsr #1
    b fp_sub_rounddown
    /* combine result */
fp_sub_normexit:
    orr r0,r0,xp1,lsl #23          		/* set exponent */
    bic mant1,mant1,#1<<23          		/* clear hi-order */
    orr r0,r0,mant1                 		/* full result */
fp_sub_exit:
    ldmfd r13!,{r2-r5,pc}

