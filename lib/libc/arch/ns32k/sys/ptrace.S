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
 * 29-Apr-92  Johannes Helander (jvh) at Helsinki University of Technology
 *	Created.
 *
 *	$Id: ptrace.S,v 1.1.1.1 1995/10/18 08:41:39 deraadt Exp $
 */

/* 
 * This is included for the NetBSD version!
 */

#include "SYS.h"

	.globl	_errno
	.globl	cerror

ENTRY(ptrace)
	PIC_PROLOGUE
	movqd	0, PIC_GOT(_errno)
	PIC_EPILOGUE
	movd	SYS_ptrace, r0
	SVC
	bcs	cerror
	ret	0
