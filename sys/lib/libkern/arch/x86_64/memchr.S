/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 * Adapted for NetBSD/x86_64 by Frank van der Linden <fvdl@wasabisystems.com>
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: memchr.S,v 1.1 2001/06/19 00:22:46 fvdl Exp $")
#endif

ENTRY(memchr)
	movb	%sil,%al		/* set character to search for */
	movq	%rdx,%rcx		/* set length of search */
	testq	%rcx,%rcx		/* test for len == 0 */
	jz	L1
	cld				/* set search forward */
	repne				/* search! */
	scasb
	jne	L1			/* scan failed, return null */
	leaq	-1(%rdi),%rax		/* adjust result of scan */
	ret
L1:	xorq	%rax,%rax
	ret
