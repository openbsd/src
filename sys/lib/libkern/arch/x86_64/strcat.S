/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 * Adapted for NetBSD/x86_64 by Frank van der Linden <fvdl@wasabisystems.com>
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: strcat.S,v 1.1 2001/06/19 00:22:47 fvdl Exp $")
#endif

/*
 * NOTE: I've unrolled the loop eight times: large enough to make a
 * significant difference, and small enough not to totally trash the
 * cache.
 */

ENTRY(strcat)
	movq	%rdi,%r11

	cld				/* set search forward */
	xorl	%eax,%eax		/* set search for null terminator */
	movq	$-1,%rcx		/* set search for lots of characters */
	repne				/* search! */
	scasb

	decq	%rdi

L1:	movb	(%rsi),%al		/* unroll loop, but not too much */
	movb	%al,(%rdi)
	testb	%al,%al
	jz	L2
	movb	1(%rsi),%al
	movb	%al,1(%rdi)
	testb	%al,%al
	jz	L2
	movb	2(%rsi),%al
	movb	%al,2(%rdi)
	testb	%al,%al
	jz	L2
	movb	3(%rsi),%al
	movb	%al,3(%rdi)
	testb	%al,%al
	jz	L2
	movb	4(%rsi),%al
	movb	%al,4(%rdi)
	testb	%al,%al
	jz	L2
	movb	5(%rsi),%al
	movb	%al,5(%rdi)
	testb	%al,%al
	jz	L2
	movb	6(%rsi),%al
	movb	%al,6(%rdi)
	testb	%al,%al
	jz	L2
	movb	7(%rsi),%al
	movb	%al,7(%rdi)
	addq	$8,%rsi
	addq	$8,%rdi
	testb	%al,%al
	jnz	L1
L2:	movq	%r11,%rax
	ret
