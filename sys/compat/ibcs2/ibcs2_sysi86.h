/*	$NetBSD: ibcs2_sysi86.h,v 1.1 1996/01/06 03:23:54 scottb Exp $	*/

/*
 * Copyright (c) 1996 Scott Bartram
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _IBCS2_SYSI86_H
#define _IBCS2_SYSI86_H

#define IBCS2_SI86FPHW		40	/* check floating-point support */
#define IBCS2_SI86STIME		54	/* set system time */
#define IBCS2_SI86SETNAME	56	/* set hostname */
#define IBCS2_SI86PHYSMEM	65	/* get physical memory size */

/* from sys/fp.h */
#define IBCS2_FP_NO	0
#define IBCS2_FP_SW	1
#define IBCS2_FP_HW	2
#define IBCS2_FP_287	2
#define IBCS2_FP_387	3

#endif /* _IBCS2_SYSI86_H */
