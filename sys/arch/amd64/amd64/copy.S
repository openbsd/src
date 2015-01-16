/*	$OpenBSD: copy.S,v 1.6 2015/01/16 10:17:51 sf Exp $	*/
/*	$NetBSD: copy.S,v 1.1 2003/04/26 18:39:26 fvdl Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "assym.h"

#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/asm.h>
#include <machine/codepatch.h>

/*
 * As stac/clac SMAP instructions are 3 bytes, we want the fastest
 * 3 byte nop sequence possible here.  This will be replaced by
 * stac/clac instructions if SMAP is detected after booting.
 *
 * This would be 'nop (%rax)' if binutils could cope.
 * Intel documents multi-byte NOP sequences as being available
 * on all family 0x6 and 0xf processors (ie 686+)
 */
#define SMAP_NOP	.byte 0x0f, 0x1f, 0x00
#define SMAP_STAC	CODEPATCH_START			;\
			SMAP_NOP			;\
			CODEPATCH_END(CPTAG_STAC)
#define SMAP_CLAC	CODEPATCH_START			;\
			SMAP_NOP			;\
			CODEPATCH_END(CPTAG_CLAC)

/*
 * Copy routines from and to userland, plus a few more. See the
 * section 9 manpages for info. Some cases can be optimized more.
 *
 * I wonder if it's worthwhile to make these use SSE2 registers.
 */

ENTRY(kcopy)
	movq	CPUVAR(CURPCB),%rax
	pushq	PCB_ONFAULT(%rax)
	leaq	_C_LABEL(copy_fault)(%rip),%r11
	movq	%r11, PCB_ONFAULT(%rax)

	xchgq	%rdi,%rsi
	movq	%rdx,%rcx

	movq	%rdi,%rax
	subq	%rsi,%rax
	cmpq	%rcx,%rax		# overlapping?
	jb	1f
	cld				# nope, copy forward
	shrq	$3,%rcx			# copy by 64-bit words
	rep
	movsq

	movq	%rdx,%rcx
	andl	$7,%ecx			# any bytes left?
	rep
	movsb

	movq	CPUVAR(CURPCB),%rdx
	popq	PCB_ONFAULT(%rdx)
	xorq	%rax,%rax
	ret

1:	addq	%rcx,%rdi		# copy backward
	addq	%rcx,%rsi
	std
	andq	$7,%rcx			# any fractional bytes?
	decq	%rdi
	decq	%rsi
	rep
	movsb
	movq	%rdx,%rcx		# copy remainder by 64-bit words
	shrq	$3,%rcx
	subq	$7,%rsi
	subq	$7,%rdi
	rep
	movsq
	cld

	movq	CPUVAR(CURPCB),%rdx
	popq	PCB_ONFAULT(%rdx)
	xorq	%rax,%rax
	ret

ENTRY(copyout)
	pushq	$0

	xchgq	%rdi,%rsi
	movq	%rdx,%rax
	
	movq	%rdi,%rdx
	addq	%rax,%rdx
	jc	_C_LABEL(copy_efault)
	movq	$VM_MAXUSER_ADDRESS,%r8
	cmpq	%r8,%rdx
	ja	_C_LABEL(copy_efault)

	movq	CPUVAR(CURPCB),%rdx
	leaq	_C_LABEL(copy_fault)(%rip),%r11
	movq	%r11,PCB_ONFAULT(%rdx)
	SMAP_STAC
	cld
	movq	%rax,%rcx
	shrq	$3,%rcx
	rep
	movsq
	movb	%al,%cl
	andb	$7,%cl
	rep
	movsb
	SMAP_CLAC
	popq	PCB_ONFAULT(%rdx)
	xorl	%eax,%eax
	ret

ENTRY(copyin)
	movq	CPUVAR(CURPCB),%rax
	pushq	$0
	leaq	_C_LABEL(copy_fault)(%rip),%r11
	movq	%r11,PCB_ONFAULT(%rax)
	SMAP_STAC
	xchgq	%rdi,%rsi
	movq	%rdx,%rax

	movq	%rsi,%rdx
	addq	%rax,%rdx
	jc	_C_LABEL(copy_efault)
	movq	$VM_MAXUSER_ADDRESS,%r8
	cmpq	%r8,%rdx
	ja	_C_LABEL(copy_efault)

3:	/* bcopy(%rsi, %rdi, %rax); */
	cld
	movq	%rax,%rcx
	shrq	$3,%rcx
	rep
	movsq
	movb	%al,%cl
	andb	$7,%cl
	rep
	movsb

	SMAP_CLAC
	movq	CPUVAR(CURPCB),%rdx
	popq	PCB_ONFAULT(%rdx)
	xorl	%eax,%eax
	ret

NENTRY(copy_efault)
	movq	$EFAULT,%rax

NENTRY(copy_fault)
	SMAP_CLAC
	movq	CPUVAR(CURPCB),%rdx
	popq	PCB_ONFAULT(%rdx)
	ret

ENTRY(copyoutstr)
	xchgq	%rdi,%rsi
	movq	%rdx,%r8
	movq	%rcx,%r9

5:	movq	CPUVAR(CURPCB),%rax
	leaq	_C_LABEL(copystr_fault)(%rip),%r11
	movq	%r11,PCB_ONFAULT(%rax)
	SMAP_STAC
	/*
	 * Get min(%rdx, VM_MAXUSER_ADDRESS-%rdi).
	 */
	movq	$VM_MAXUSER_ADDRESS,%rax
	subq	%rdi,%rax
	jbe	_C_LABEL(copystr_efault)	/* die if CF == 1 || ZF == 1 */
	cmpq	%rdx,%rax
	jae	1f
	movq	%rax,%rdx
	movq	%rax,%r8

1:	incq	%rdx
	cld

1:	decq	%rdx
	jz	2f
	lodsb
	stosb
	testb	%al,%al
	jnz	1b

	/* Success -- 0 byte reached. */
	decq	%rdx
	xorq	%rax,%rax
	jmp	copystr_return

2:	/* rdx is zero -- return EFAULT or ENAMETOOLONG. */
	movq	$VM_MAXUSER_ADDRESS,%r11
	cmpq	%r11,%rdi
	jae	_C_LABEL(copystr_efault)
	movq	$ENAMETOOLONG,%rax
	jmp	copystr_return

ENTRY(copyinstr)
	xchgq	%rdi,%rsi
	movq	%rdx,%r8
	movq	%rcx,%r9

	movq	CPUVAR(CURPCB),%rcx
	leaq	_C_LABEL(copystr_fault)(%rip),%r11
	movq	%r11,PCB_ONFAULT(%rcx)
	SMAP_STAC

	/*
	 * Get min(%rdx, VM_MAXUSER_ADDRESS-%rsi).
	 */
	movq	$VM_MAXUSER_ADDRESS,%rax
	subq	%rsi,%rax
	jbe	_C_LABEL(copystr_efault)	/* die if CF == 1 || ZF == 1 */
	cmpq	%rdx,%rax
	jae	1f
	movq	%rax,%rdx
	movq	%rax,%r8

1:	incq	%rdx
	cld

1:	decq	%rdx
	jz	2f
	lodsb
	stosb
	testb	%al,%al
	jnz	1b

	/* Success -- 0 byte reached. */
	decq	%rdx
	xorq	%rax,%rax
	jmp	copystr_return

2:	/* edx is zero -- return EFAULT or ENAMETOOLONG. */
	movq	$VM_MAXUSER_ADDRESS,%r11
	cmpq	%r11,%rsi
	jae	_C_LABEL(copystr_efault)
	movq	$ENAMETOOLONG,%rax
	jmp	copystr_return

ENTRY(copystr_efault)
	movl	$EFAULT,%eax

ENTRY(copystr_fault)
copystr_return:
	SMAP_CLAC
	/* Set *lencopied and return %eax. */
	movq	CPUVAR(CURPCB),%rcx
	movq	$0,PCB_ONFAULT(%rcx)
	testq	%r9,%r9
	jz	8f
	subq	%rdx,%r8
	movq	%r8,(%r9)

8:	ret

ENTRY(copystr)
	xchgq	%rdi,%rsi
	movq	%rdx,%r8

	incq	%rdx
	cld

1:	decq	%rdx
	jz	4f
	lodsb
	stosb
	testb	%al,%al
	jnz	1b

	/* Success -- 0 byte reached. */
	decq	%rdx
	xorl	%eax,%eax
	jmp	6f

4:	/* edx is zero -- return ENAMETOOLONG. */
	movl	$ENAMETOOLONG,%eax

6:	/* Set *lencopied and return %eax. */
	testq	%rcx,%rcx
	jz	7f
	subq	%rdx,%r8
	movq	%r8,(%rcx)

7:	ret

.globl _C_LABEL(_stac)
_C_LABEL(_stac):
	stac

.globl _C_LABEL(_clac)
_C_LABEL(_clac):
	clac
