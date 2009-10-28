/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(ldiv)
	movl	4(%esp),%eax
	movl	8(%esp),%ecx
	cdq
	idiv	%ecx
	movl	%eax,4(%esp)
	movl	%edx,8(%esp)
	ret
