/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: index.S,v 1.2 1996/08/19 08:12:59 tholo Exp $"
#endif

#ifdef STRCHR
ENTRY(strchr)
#else
ENTRY(index)
#endif
	pushl	%ebx
	movl	8(%esp),%eax
	movb	12(%esp),%cl
	.align 2,0x90
L1:
	movb	(%eax),%bl
	cmpb	%bl,%cl			/* found char??? */
	je 	L2
	incl	%eax
	testb	%bl,%bl			/* null terminator??? */
	jnz	L1
	xorl	%eax,%eax
L2:
	popl	%ebx
	ret
