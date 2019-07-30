/*	$OpenBSD: fpgetmask.S,v 1.2 2015/05/29 08:50:12 uebayasi Exp $	*/
/*	$NetBSD: fpgetmask.S,v 1.3 2002/06/12 19:17:22 fvdl Exp $	*/

/*
 * Written by J.T. Conklin, Apr 4, 1995
 * Public domain.
 */

#include <machine/asm.h>

/*
 * XXX only read x87 control word here. If an application only
 * uses the fp* interface to manipulate FP bits, it should
 * always remain in sync with the SSE mxcsr register.
 */

#ifdef WEAK_ALIAS
WEAK_ALIAS(fpgetmask, _fpgetmask)
ENTRY(_fpgetmask)
#else
ENTRY(fpgetmask)
#endif
	fnstcw -4(%rsp)
	movl -4(%rsp),%eax
	notl %eax
	andl $63,%eax
	ret
#ifdef WEAK_ALIAS
END(_fpgetmask)
#else
END(fpgetmask)
#endif
