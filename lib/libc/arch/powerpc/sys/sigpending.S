/*
 * COPYRIGHT
 */
#if defined(SYSLIBC_SCCS)
	.text
	.asciz "$OpenBSD: sigpending.S,v 1.2 1998/08/07 01:44:06 rahnds Exp $"
#endif /* SYSLIBC_SCCS */

#include "SYS.h"

RSYSCALL(sigpending)
