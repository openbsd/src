/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

/*
 * XXXfvdl split this file.
 */

RCSID("$NetBSD: s_copysignf.S,v 1.5 2003/07/26 19:25:01 salo Exp $")

#ifdef __x86_64__
.Lneg:
	.long 0x7fffffff
.Lpos:
	.long 0x80000000
#endif

ENTRY(copysignf)
#ifdef __i386__
	movl	8(%esp),%edx
	andl	$0x80000000,%edx
	movl	4(%esp),%eax
	andl	$0x7fffffff,%eax
	orl	%edx,%eax
	movl	%eax,4(%esp)
	flds	4(%esp)
#else
#if 0
	/*
	 * XXXfvdl gas doesn't grok this.
	 * but it's legal according to the p4 manual.
	 */
	movss    .Lpos(%rip),%xmm2
	movss    .Lneg(%rip),%xmm3
	pandq   %xmm2,%xmm1
	pandq   %xmm3,%xmm0
	porq    %xmm1,%xmm0
#else
	movss	%xmm0,-4(%rsp)
	movss	%xmm1,-8(%rsp)
	movl	-8(%rsp),%edx
	andl	$0x80000000,%edx
	movl	-4(%rsp),%eax
	andl	$0x7fffffff,%eax
	orl	%edx,%eax
	movl	%eax,-4(%rsp)
	movss	-4(%rsp),%xmm0
#endif
#endif
	ret
