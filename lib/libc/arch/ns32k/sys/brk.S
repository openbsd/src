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
/*
 * HISTORY
 * 11-May-92  Johannes Helander (jvh) at Helsinki University of Technology
 *	Created.
 *
 *	$Id: brk.S,v 1.1.1.1 1995/10/18 08:41:39 deraadt Exp $
 */

#include "SYS.h"

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
