/* 
 * Copyright (c) 1992 Helsinki University of Technology
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * HELSINKI UNIVERSITY OF TECHNOLOGY ALLOWS FREE USE OF THIS SOFTWARE IN
 * ITS "AS IS" CONDITION. HELSINKI UNIVERSITY OF TECHNOLOGY DISCLAIMS ANY
 * LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
 * USE OF THIS SOFTWARE.
 */

#include "SYS.h"

#if defined(SYSLIBC_SCCS)
	.text
	.asciz "$OpenBSD: cerror.S,v 1.2 1996/08/19 08:17:06 tholo Exp $"
#endif /* SYSLIBC_SCCS */

	.globl  cerror
	.globl	_errno

cerror:
	PIC_PROLOGUE
	movd	r0, PIC_GOT(_errno)
	PIC_EPILOGUE
	movqd	-1, r0
	movqd	-1, r1
	ret	0
