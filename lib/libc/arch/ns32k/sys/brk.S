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
	.asciz "$OpenBSD: brk.S,v 1.2 1996/08/19 08:17:05 tholo Exp $"
#endif /* SYSLIBC_SCCS */

	.globl	_curbrk
	.globl	minbrk

ENTRY(_brk)
	br	ok2

ENTRY(brk)
	PIC_PROLOGUE
	cmpd	PIC_S_ARG0, PIC_GOT(minbrk)
	bge	ok1
	movd	PIC_GOT(minbrk), PIC_S_ARG0
ok1:
	PIC_EPILOGUE
ok2:
	movd	SYS_break, r0
	SVC
	bcs	cerror
	PIC_PROLOGUE
	movd	PIC_S_ARG0, PIC_GOT(_curbrk)
	movqd	0, r0
	PIC_EPILOGUE
	ret	0
