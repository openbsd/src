/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 * Adapted for NetBSD/x86_64 by Frank van der Linden <fvdl@wasabisystems.com>
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: index.S,v 1.1 2001/06/19 00:22:46 fvdl Exp $")
#endif

#ifdef STRCHR
ENTRY(strchr)
#else
ENTRY(index)
#endif
	movq	%rdi,%rax
	movb	%sil,%cl
L1:
	movb	(%rax),%dl
	cmpb	%dl,%cl			/* found char? */
	je 	L2
	incq	%rax
	testb	%dl,%dl			/* null terminator? */
	jnz	L1
	xorq	%rax,%rax
L2:
	ret
