/*	$OpenBSD: in_cksum.s,v 1.8 2008/06/26 05:42:10 ray Exp $	*/
/*	$NetBSD: in_cksum.S,v 1.2 2003/08/07 16:27:54 agc Exp $	*/

/*-
 * Copyright (c) 1998, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>
#include "assym.h"

/* LINTSTUB: include <sys/types.h> */
/* LINTSTUB: include <machine/param.h> */
/* LINTSTUB: include <sys/mbuf.h> */
/* LINTSTUB: include <netinet/in.h> */

/*
 * Checksum routine for Internet Protocol family headers.
 *
 * in_cksum(m, len)
 *
 * Registers used:
 * %eax = sum
 * %ebx = m->m_data
 * %cl = rotation count to unswap
 * %edx = m->m_len
 * %ebp = m
 * %esi = len
 */

#define	SWAP \
	roll	$8, %eax	; \
	xorb	$8, %cl

#define	UNSWAP \
	roll	%cl, %eax

#define	MOP \
	adcl	$0, %eax

#define	ADVANCE(n) \
	leal	n(%ebx), %ebx	; \
	leal	-n(%edx), %edx	; \

#define	ADDBYTE \
	SWAP			; \
	addb	(%ebx), %ah

#define	ADDWORD \
	addw	(%ebx), %ax

#define	ADD(n) \
	addl	n(%ebx), %eax

#define	ADC(n) \
	adcl	n(%ebx), %eax

#define	REDUCE \
	movzwl	%ax, %edx	; \
	shrl	$16, %eax	; \
	addw	%dx, %ax	; \
	adcw	$0, %ax


/* LINTSTUB: Func: int in4_cksum(struct mbuf *m, u_int8_t nxt, int off, int len) */
ENTRY(in4_cksum)
	pushl	%ebp
	pushl	%ebx
	pushl	%esi

	movl	16(%esp), %ebp
	movzbl	20(%esp), %eax		/* sum = nxt */
	movl	24(%esp), %edx		/* %edx = off */
	movl	28(%esp), %esi		/* %esi = len */
	testl	%eax, %eax
	jz	.Lmbuf_loop_0		/* skip if nxt == 0 */
	movl	M_DATA(%ebp), %ebx
	addl	%esi, %eax		/* sum += len */
	shll	$8, %eax		/* sum = htons(sum) */

	ADD(IP_SRC)			/* sum += ip->ip_src */
	ADC(IP_DST)			/* sum += ip->ip_dst */
	MOP
.Lmbuf_loop_0:
	testl	%ebp, %ebp
	jz	.Lout_of_mbufs

	movl	M_DATA(%ebp), %ebx	/* %ebx = m_data */
	movl	M_LEN(%ebp), %ecx	/* %ecx = m_len */
	movl	M_NEXT(%ebp), %ebp

	subl	%ecx, %edx		/* %edx = off - m_len */
	jnb	.Lmbuf_loop_0

	addl	%edx, %ebx		/* %ebx = m_data + off - m_len */
	negl	%edx			/* %edx = m_len - off */
	addl	%ecx, %ebx		/* %ebx = m_data + off */
	xorb	%cl, %cl

	/*
	 * The len == 0 case is handled really inefficiently, by going through
	 * the whole short_mbuf path once to get back to mbuf_loop_1 -- but
	 * this case never happens in practice, so it's sufficient that it
	 * doesn't explode.
	 */
	jmp	.Lin4_entry


/* LINTSTUB: Func: int in_cksum(struct mbuf *m, int len) */
ENTRY(in_cksum)
	pushl	%ebp
	pushl	%ebx
	pushl	%esi

	movl	16(%esp), %ebp
	movl	20(%esp), %esi
	xorl	%eax, %eax
	xorb	%cl, %cl

.Lmbuf_loop_1:
	testl	%esi, %esi
	jz	.Ldone

.Lmbuf_loop_2:
	testl	%ebp, %ebp
	jz	.Lout_of_mbufs

	movl	M_DATA(%ebp), %ebx
	movl	M_LEN(%ebp), %edx
	movl	M_NEXT(%ebp), %ebp

.Lin4_entry:
	cmpl	%esi, %edx
	jbe	1f
	movl	%esi, %edx

1:
	subl	%edx, %esi

	cmpl	$32, %edx
	jb	.Lshort_mbuf

	testb	$3, %bl
	jz	.Ldword_aligned

	testb	$1, %bl
	jz	.Lbyte_aligned

	ADDBYTE
	ADVANCE(1)
	MOP

	testb	$2, %bl
	jz	.Lword_aligned

.Lbyte_aligned:
	ADDWORD
	ADVANCE(2)
	MOP

.Lword_aligned:
.Ldword_aligned:
	testb	$4, %bl
	jnz	.Lqword_aligned

	ADD(0)
	ADVANCE(4)
	MOP

.Lqword_aligned:
	testb	$8, %bl
	jz	.Loword_aligned

	ADD(0)
	ADC(4)
	ADVANCE(8)
	MOP

.Loword_aligned:
	subl	$128, %edx
	jb	.Lfinished_128

.Lloop_128:
	ADD(12)
	ADC(0)
	ADC(4)
	ADC(8)
	ADC(28)
	ADC(16)
	ADC(20)
	ADC(24)
	ADC(44)
	ADC(32)
	ADC(36)
	ADC(40)
	ADC(60)
	ADC(48)
	ADC(52)
	ADC(56)
	ADC(76)
	ADC(64)
	ADC(68)
	ADC(72)
	ADC(92)
	ADC(80)
	ADC(84)
	ADC(88)
	ADC(108)
	ADC(96)
	ADC(100)
	ADC(104)
	ADC(124)
	ADC(112)
	ADC(116)
	ADC(120)
	leal	128(%ebx), %ebx
	MOP

	subl	$128, %edx
	jnb	.Lloop_128

.Lfinished_128:
	subl	$32-128, %edx
	jb	.Lfinished_32

.Lloop_32:
	ADD(12)
	ADC(0)
	ADC(4)
	ADC(8)
	ADC(28)
	ADC(16)
	ADC(20)
	ADC(24)
	leal	32(%ebx), %ebx
	MOP

	subl	$32, %edx
	jnb	.Lloop_32

.Lfinished_32:
.Lshort_mbuf:
	testb	$16, %dl
	jz	.Lfinished_16

	ADD(12)
	ADC(0)
	ADC(4)
	ADC(8)
	leal	16(%ebx), %ebx
	MOP

.Lfinished_16:
	testb	$8, %dl
	jz	.Lfinished_8

	ADD(0)
	ADC(4)
	leal	8(%ebx), %ebx
	MOP

.Lfinished_8:
	testb	$4, %dl
	jz	.Lfinished_4

	ADD(0)
	leal	4(%ebx), %ebx
	MOP

.Lfinished_4:
	testb	$3, %dl
	jz	.Lmbuf_loop_1

	testb	$2, %dl
	jz	.Lfinished_2

	ADDWORD
	leal	2(%ebx), %ebx
	MOP

	testb	$1, %dl
	jz	.Lfinished_1

.Lfinished_2:
	ADDBYTE
	MOP

.Lfinished_1:
.Lmbuf_done:
	testl	%esi, %esi
	jnz	.Lmbuf_loop_2

.Ldone:
	UNSWAP
	REDUCE
	notw	%ax

.Lreturn:
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret

.Lout_of_mbufs:
	pushl	$1f
	call	_C_LABEL(printf)
	leal	4(%esp), %esp
	jmp	.Lreturn
1:
	.asciz	"cksum: out of data\n"
