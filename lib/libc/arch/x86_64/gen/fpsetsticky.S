/*	$OpenBSD: fpsetsticky.S,v 1.1 2004/01/28 01:44:45 mickey Exp $	*/
/*	$NetBSD: fpsetsticky.S,v 1.3 2002/06/12 19:17:23 fvdl Exp $	*/

/*
 * Written by Frank van der Linden at Wasabi Systems for NetBSD
 * Public domain.
 */

#include <machine/asm.h>

/*
 * XXX set both the x87 status word and the SSE mxcsr register.
 * Applications should only set exception and round flags
 * via the fp*() interface, otherwise the status words
 * will get our of sync.
 */ 


#ifdef WEAK_ALIAS
WEAK_ALIAS(fpsetsticky, _fpsetsticky)
ENTRY(_fpsetsticky)
#else
ENTRY(fpsetsticky)
#endif
	fnstenv	-28(%rsp)
	stmxcsr	-32(%rsp)

	andl	$63,%edi

	movl	-24(%rsp),%eax
	movl	%eax,%edx
	andl	$63,%eax

	orl	%edi,%edx
	movl	%edx,-24(%rsp)

	movl	-32(%rsp),%edx
	orl	%edi,%edx
	movl	%edx,-32(%rsp)

	ldmxcsr	-32(%rsp)
	fldenv	-28(%rsp)
	ret
