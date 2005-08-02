/*	$OpenBSD: s_cosf.S,v 1.2 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

/* A float's domain isn't large enough to require argument reduction. */
ENTRY(cosf)
	flds	4(%esp)
	fcos
	ret	
