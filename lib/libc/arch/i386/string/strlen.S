/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: strlen.S,v 1.2 1996/08/19 08:13:19 tholo Exp $"
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
