/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: ldiv.S,v 1.2 1996/08/19 08:12:48 tholo Exp $"
#endif

ENTRY(ldiv)
        movl    4(%esp),%eax
        movl    8(%esp),%ecx
        cdq
        idiv    %ecx
        movl    %eax,4(%esp)
        movl    %edx,8(%esp)
        ret
