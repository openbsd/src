/*	$OpenBSD: memcpy.S,v 1.3 1996/11/13 22:05:08 niklas Exp $	*/
/*	$NetBSD: memcpy.S,v 1.1 1995/08/13 00:40:47 cgd Exp $	*/

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: memcpy.S,v 1.3 1996/11/13 22:05:08 niklas Exp $"
#endif

#define MEMCOPY
#include "bcopy.S"
