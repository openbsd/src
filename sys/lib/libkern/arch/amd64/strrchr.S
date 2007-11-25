/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 * Adapted for NetBSD/x86_64 by Frank van der Linden <fvdl@wasabisystems.com>
 */

#include <machine/asm.h>

ENTRY(strrchr)
	movb	%sil,%cl
	xorq	%rax,%rax		/* init pointer to null */
L1:
	movb	(%rdi),%dl
	cmpb	%dl,%cl
	jne	L2
	movq	%rdi,%rax
L2:
	incq	%rdi
	testb	%dl,%dl			/* null terminator??? */
	jnz	L1
	ret
