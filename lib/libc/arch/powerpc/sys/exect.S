/*
 * COPYRIGHT
 */
#if defined(SYSLIBC_SCCS)
	.text
	.asciz "$OpenBSD: exect.S,v 1.4 2002/10/07 04:16:34 drahn Exp $"
#endif /* SYSLIBC_SCCS */

#include "SYS.h"

	.text
PREFIX2(exect,execve)
	sc

	/* check for error */
	cmpwi	0, 0
	beq+	_ASM_LABEL(exect_ok) 
	b	PIC_PLT(_ASM_LABEL(__cerror))

	/* update, curbrk and return */
exect_ok:
	blr
