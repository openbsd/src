/* $arla: process.x86_64.S,v 1.1 2003/04/27 10:57:25 haba Exp $ */

/*
 * Copyright (c) 2003 Kungliga Tekniska Högskolan
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

#include <config.h>

#undef RCSID

/* x86_64 Assembly
 *
 * By Harald Barth <haba@stacken.kth.se> after looking
 * at Derek Atkins' i386 routines and realizing that
 * there were some differences and it was not enough
 * just renaming the registers.
 */
	
#ifdef HAVE_MACHINE_ASM_H
#include <machine/asm.h>
#endif

#include <lwp_elf.h>
	
	.file "process.s"
	.data
	.text

/*
 * struct savearea {
 *    char    *topstack;
 * }
 */

	.set    topstack,0

/*
 * savecontext(int (*f)(), struct savearea *area1, char *newsp)
 */

/* 
 * In spite of passing arguments in registers, gcc first copies the content of the
 * registers onto the stack. I do not know why gcc does this, but for now I mimic
 * gcc's behaviour. Here are the offsets the arguments are copied to.
 */
	.set    f,-8
	.set    area1,-16
	.set    newsp,-24

.globl		_C_LABEL(PRE_Block)
.globl		_C_LABEL(savecontext)

ENTRY(savecontext)
	pushq   %rbp                    /* The frame setup is just like gcc */
	movq    %rsp,%rbp
	subq	$32, %rsp		/* make room for args on the stack */
	movq	%rdi, f(%rbp)		/* (3*8=24 but increments seem to */
	movq	%rsi, area1(%rbp)	/* i multiples of 24, so 32 it is) */
	movq	%rdx, newsp(%rbp)	/* and copy them there. */

	movl    $1,_C_LABEL(PRE_Block)  /* Do not allow any interrupts */

	pushq	%rsp			/* Push all registers onto the stack */
	pushq	%rax			/* Probably not _all_ are necessary */
	pushq	%rcx			/* but better one too much than one */
	pushq	%rdx			/* forgotten */
	pushq	%rbx
	pushq	%rbp
	pushq	%rsi
	pushq	%rdi
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15			/* Btw, the pusha instruction is no more */

	movq    area1(%rbp),%rax        /* rax = base of savearea */
	movq    %rsp,topstack(%rax)	/* area->topstack = rsp */
	movq    newsp(%rbp),%rax        /* rax = new sp */
	cmpq    $0,%rax
	je      L1                      /* if new sp is 0 then dont change rsp */
	movq    %rax,%rsp               /* Change rsp to the new sp */
L1:
	jmp     *f(%rbp)                /* jump to function pointer passed in arg */

/* Shouldnt be here....*/
	call    _C_LABEL(abort)

/*
 * returnto(struct savearea *area2)
 */

/* Offset where we put arg - se savecontext */
	.set    area2,-8

.globl		_C_LABEL(returnto)

ENTRY(returnto)
	pushq   %rbp			/* New frame, gcc style */
	movq    %rsp, %rbp              /* See savecontext above */
	subq	$16, %rsp		/* Make room for 2 args */
	movq	%rdi, area2(%rbp)	/* use room to copy 1 arg */
	movq    area2(%rbp),%rax        /* rax = area2 */
	movq    topstack(%rax),%rsp	/* restore rsp from place relative rbp*/

	popq	%r15			/* Restore regs */
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rdi
	popq	%rsi
	popq	%rbp
	popq	%rbx
	popq	%rdx
	popq	%rcx
	popq	%rax
	popq	%rsp			/* See savecontext */
	
	movl    $0,_C_LABEL(PRE_Block)  /* clear it up... */
	addq	$32, %rsp		/* We did rsp-32 above, correct that */
	popq    %rbp
	ret
	
/* We never should get here, put in emergency brake as in i386 code */
	pushq   $1234
	call    _C_LABEL(abort)

