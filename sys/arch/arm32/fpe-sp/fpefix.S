/* $NetBSD: fpefix.S,v 1.2 1996/03/18 19:58:15 mark Exp $ */

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
 * fpefix.S
 *
 * FP register to integer
 *
 * Created      : 11/02/95
 */

/* std. registers
 */
sp	.req	r13
lr	.req	r14
pc	.req	r15


mant1	.req	r0
xp1	.req	r1

    .global _nc_fp_fix

_nc_fp_fix:
    cmp r0,#0
    moveq pc,r14
    mov xp1,r0,lsr #23
    bic mant1,r0,xp1,lsl #23
    orr mant1,mant1,#1<<23
    tst xp1,#1<<8
    rsbne mant1,mant1,#0
    bic xp1,xp1,#1<<8
    subs xp1,xp1,#127
    movmi r0,#0
    movmi pc,r14
    rsbs xp1,xp1,#23
    movpl mant1,mant1,asr xp1
    rsbmi xp1,xp1,#0
    movmi mant1,mant1,asl xp1
    mov pc,r14
