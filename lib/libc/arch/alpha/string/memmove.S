/*	$OpenBSD: memmove.S,v 1.3 1996/11/13 22:05:09 niklas Exp $	*/
/*	$NetBSD: memmove.S,v 1.1 1995/08/13 00:40:48 cgd Exp $	*/

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: memmove.S,v 1.3 1996/11/13 22:05:09 niklas Exp $"
#endif

#define MEMMOVE
#include "bcopy.S"
