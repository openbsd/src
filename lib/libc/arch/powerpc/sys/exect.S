/*
 * COPYRIGHT
 */
#if defined(SYSLIBC_SCCS)
	.text
	.asciz "$OpenBSD: exect.S,v 1.3 1999/01/28 05:09:12 rahnds Exp $"
#endif /* SYSLIBC_SCCS */

#include "SYS.h"

	.text
PREFIX2(exect,execve)
	sc

	/* check for error */
	cmpwi	0, 0
	beq+	_ASM_LABEL(exect_ok) 
	b	PIC_PLT(_ASM_LABEL(cerror))

	/* update, curbrk and return */
exect_ok:
	blr
