/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: strlen.S,v 1.5 1995/10/07 09:27:12 mycroft Exp $")
#endif

ENTRY(strlen)
	pushl	%edi
	movl	8(%esp),%edi		/* string address */
	cld				/* set search forward */
	xorl	%eax,%eax		/* set search for null terminator */
	movl	$-1,%ecx		/* set search for lots of characters */
	repne				/* search! */
	scasb
	notl	%ecx			/* get length by taking	complement */
	leal	-1(%ecx),%eax		/* and subtracting one */
	popl	%edi
	ret
