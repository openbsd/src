/*	$OpenBSD: memchr.S,v 1.1 1997/11/04 19:08:13 chuck Exp $	*/

/* 
 * Written by Matthias Pfaller, 1996
 * Public domain.
 */

#include <machine/asm.h>


/*
 * void *
 * memchr (const void *b, int c, size_t len)
 *	locate the first occurrence of c in string b.
 */

ENTRY(memchr)
	enter	[r4],0
	movd	B_ARG2,r0
	movb	B_ARG1,r4
	movd	B_ARG0,r1
	skpsb	u
	bfc	0f
	movd	r1,r0
0:	exit	[r4]
	ret	0
