/*
 * Copyright (c) 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Linux for S/390 (31 bit)
 *
 * Written by Neale Ferguson <Neale.Ferguson@SoftwareAG-usa.com>
 *
 *  additional munging by Adam Thornton <adam@sinenomine.net>
 */
	        .file   "process.s"

	        .globl savecontext
	        .type  savecontext,%function
	/*
	 * savecontext(f, area1, newsp)
	 *      int (*f)()      ;   struct savearea *area1; char *newsp;
	 * f     - r2
	 * area1 - r3
	 * newsp - r4
	 */

	 /*
	  * struct savearea {
	  *      char    *topstack;
	  * }
	  */

P_PRE:	          .long   PRE_Block
P_ABORT:	        .long   abort

savecontext:
	        stm     %r7,%r15,28(%r15)       /* Save our registers */
	        lr      %r1,%r15
	        ahi     %r15,-96
	        st      %r1,0(%r15)

	/*        larl    %r5,P_PRE               /* Get A((PRE_Block)) */
	        bras    %r5,.L0                 /* Get A(A(PRE_Block)) */
	        .long   PRE_Block
	.L0:
	        l       %r5,0(%r5)              /* Get A(PRE_Block) */
	        mvi     0(%r5),1                /* Set it */
	        st      %r15,0(%r3)             /* Save stack pointer */
	        ltr     %r4,%r4                 /* If new sp is 0 */
	        jz      .L1                     /* ... don't change sp */
	        lr      %r15,%r4                /* Set new stack pointer */
	.L1:
	        br      %r2                     /* Call the routine */

	        /* Can't get here....*/

	/*        larl    %r5,P_ABORT */
	        bras    %r5,.L2
	        .long   abort
	.L2:
	        l       %r5,0(%r5)
	        basr    %r14,%r5

	.savecontext_end:
	        .size   savecontext,.savecontext_end-savecontext

	/*
	 * returnto(area2)
	 *      struct savearea *area2;
	 *
	 * area2 - r2
	 */

	        .globl  returnto
	        .type   returnto,%function

returnto:
	        l       %r15,0(%r2)             /* New frame, to get correct pointer
	*/
	/*        larl    %r5,P_PRE               /* Get A((PRE_Block)) */
	        bras    %r5,.L3                 /* Get A(A(PRE_Block)) */
	        .long   PRE_Block
	.L3:
	        l       %r5,0(%r5)              /* Get A(PRE_Block) */
	        mvi     0(%r5),0                /* Clear it */
	        l       %r15,0(%r15)            /* Point to next stack frame */
	        lm      %r7,%r15,28(%r15)       /* Restore registers */
	        br      %r14                    /* Return */

	        /* Can't happen */

	        la      %r2,1234
	/*        larl    %r9,P_ABORT */
	        bras    %r9,.L4
	        .long   abort
	.L4:
	        l       %r9,0(%r9)
	        basr    %r14,%r9

	.returnto_end:
	        .size   returnto,.returnto_end-returnto
	
