/* -*- text -*- */
/*
 * Copyright (c) 1998 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software 
 *    must display the following acknowledgement: 
 *      This product includes software developed by Kungliga Tekniska 
 *      Högskolan and its contributors. 
 *
 * 4. Neither the name of the Institute nor the names of its contributors 
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

/* $arla: process-vpp.s,v 1.1 1998/10/25 19:41:15 joda Exp $ */

/* LWP context switch for Fujitsu UXP/V */

#define registers	8 /* leave room for fp, and ra */
#define floats		(registers + 12 * 4)
#ifdef SAVE_FLOATS
#define FRAMESIZE	(floats + 15 * 8)
#else
#define FRAMESIZE 	floats
#endif

/* 
void savecontext(int (*)(void), struct savearea*, void*);
*/

        .section .text
	.global savecontext
	.align 8
savecontext:
	/* make space */
	or	%gr1, %gr0, %gr2	& st	%gr2, %gr1, 4
	addi23	%gr1, -FRAMESIZE, %gr1	& st	%gr3, %gr2, %gr0
	
	/* save registers */
	/* XXX save more registers? */
	st	%gr4,  %gr1, registers + 0
	st	%gr5,  %gr1, registers + 4
	st	%gr6,  %gr1, registers + 8
	st	%gr7,  %gr1, registers + 12
	st	%gr8,  %gr1, registers + 16
	st	%gr9,  %gr1, registers + 20
	st	%gr10, %gr1, registers + 24
	st	%gr11, %gr1, registers + 28
	st	%gr12, %gr1, registers + 32
	st	%gr13, %gr1, registers + 36
	st	%gr14, %gr1, registers + 40
	st	%gr15, %gr1, registers + 44

#ifdef SAVE_FLOATS
	/* save floting point registers */
	st.d	%fr1,  %gr1, floats + 0
	st.d	%fr2,  %gr1, floats + 8
	st.d	%fr3,  %gr1, floats + 16
	st.d	%fr4,  %gr1, floats + 24
	st.d	%fr5,  %gr1, floats + 32
	st.d	%fr6,  %gr1, floats + 40
	st.d	%fr7,  %gr1, floats + 48
	st.d	%fr8,  %gr1, floats + 56
	st.d	%fr9,  %gr1, floats + 64
	st.d	%fr10, %gr1, floats + 72
	st.d	%fr11, %gr1, floats + 80
	st.d	%fr12, %gr1, floats + 88
	st.d	%fr13, %gr1, floats + 96
	st.d	%fr14, %gr1, floats + 104
	st.d	%fr15, %gr1, floats + 112
#endif

	/* block */
	addi5	%gr0, 1, %gr20
	seti	PRE_Block, %gr21
	st.c	%gr20, %gr21, 0

	/* save sp */
	st	%gr1, %gr17, 0
	/* new sp */
	subi5	%gr18, 0, %gr0
	brc %iccp.z, samestack

	/* switch to new stack */
	or	%gr18, %gr0, %gr1
	/* load fp */
	ld	%gr1, 0, %gr2

samestack:
	/* is this necessary? */
	or	%gr16, %gr0, %gr3
	jmp	%gr3

	.type	savecontext,@function
	.size	savecontext,.-savecontext
 
/* 
void returnto(struct savearea*);
*/
	.global returnto
	.align 8
returnto:
	seti	PRE_Block, %gr17
	st.c	%gr0, %gr17, 0
	/* restore sp */
	ld	%gr16, 0, %gr1
	/* restore registers */
	ld	%gr1, registers + 0,  %gr4
	ld	%gr1, registers + 4,  %gr5
	ld	%gr1, registers + 8,  %gr6
	ld	%gr1, registers + 12, %gr7
	ld	%gr1, registers + 16, %gr8
	ld	%gr1, registers + 20, %gr9
	ld	%gr1, registers + 24, %gr10
	ld	%gr1, registers + 28, %gr11
	ld	%gr1, registers + 32, %gr12
	ld	%gr1, registers + 36, %gr13
	ld	%gr1, registers + 40, %gr14
	ld	%gr1, registers + 44, %gr15

#ifdef SAVE_FLOATS
	/* restore floting point registers */
	ld.d	%gr1, floats + 0,   %fr1
	ld.d	%gr1, floats + 8,   %fr2
	ld.d	%gr1, floats + 16,  %fr3
	ld.d	%gr1, floats + 24,  %fr4
	ld.d	%gr1, floats + 32,  %fr5
	ld.d	%gr1, floats + 40,  %fr6
	ld.d	%gr1, floats + 48,  %fr7
	ld.d	%gr1, floats + 56,  %fr8
	ld.d	%gr1, floats + 64,  %fr9
	ld.d	%gr1, floats + 72,  %fr10
	ld.d	%gr1, floats + 80,  %fr11
	ld.d	%gr1, floats + 88,  %fr12
	ld.d	%gr1, floats + 96,  %fr13
	ld.d	%gr1, floats + 104, %fr14
	ld.d	%gr1, floats + 112, %fr15
#endif
	addi23	%gr1, FRAMESIZE,  %gr1
	ld	%gr1, 0, %gr3
	ld	%gr1, 4, %gr2

	jmp	%gr3

	.type	returnto,@function
	.size	returnto,.-returnto
