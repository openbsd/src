/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: s_finitef.S,v 1.3 1995/05/09 00:00:02 jtc Exp $")

ENTRY(finitef)
	movl	4(%esp),%eax
	andl	$0x7ff00000, %eax
	cmpl	$0x7ff00000, %eax
	setnel	%al
	andl	$0x000000ff, %eax
	ret
