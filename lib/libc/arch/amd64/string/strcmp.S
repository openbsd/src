/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 * Adapted for NetBSD/x86_64 by Frank van der Linden <fvdl@wasabisystems.com>
 */

#include <machine/asm.h>

/*
 * NOTE: I've unrolled the loop eight times: large enough to make a
 * significant difference, and small enough not to totally trash the
 * cache.
 */

ENTRY(strcmp)
	jmp	L2			/* Jump into the loop. */

L1:	incq	%rdi
	incq	%rsi
L2:	movb	(%rdi),%cl
	testb	%cl,%cl			/* null terminator */
	jz	L3
	cmpb	%cl,(%rsi)		/* chars match */
	jne	L3

	incq	%rdi
	incq	%rsi
	movb	(%rdi),%cl
	testb	%cl,%cl
	jz	L3
	cmpb	%cl,(%rsi)
	jne	L3

	incq	%rdi
	incq	%rsi
	movb	(%rdi),%cl
	testb	%cl,%cl
	jz	L3
	cmpb	%cl,(%rsi)
	jne	L3

	incq	%rdi
	incq	%rsi
	movb	(%rdi),%cl
	testb	%cl,%cl
	jz	L3
	cmpb	%cl,(%rsi)
	jne	L3

	incq	%rdi
	incq	%rsi
	movb	(%rdi),%cl
	testb	%cl,%cl
	jz	L3
	cmpb	%cl,(%rsi)
	jne	L3

	incq	%rdi
	incq	%rsi
	movb	(%rdi),%cl
	testb	%cl,%cl
	jz	L3
	cmpb	%cl,(%rsi)
	jne	L3

	incq	%rdi
	incq	%rsi
	movb	(%rdi),%cl
	testb	%cl,%cl
	jz	L3
	cmpb	%cl,(%rsi)
	jne	L3

	incq	%rdi
	incq	%rsi
	movb	(%rdi),%cl
	testb	%cl,%cl
	jz	L3
	cmpb	%cl,(%rsi)
	je	L1
L3:	movzbl	(%rdi),%eax		/* unsigned comparison */
	movzbl	(%rsi),%edx
	subl	%edx,%eax
	ret
